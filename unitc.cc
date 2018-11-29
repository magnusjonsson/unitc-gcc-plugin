#include <stdlib.h>
#include <gcc-plugin.h>
#include <plugin-version.h>
#include <tree.h>
#include <tree-iterator.h>
#include <tree-pretty-print.h>
#include <stringpool.h>
#include <attribs.h>
#include <diagnostic.h>

#include <assert.h>

__attribute__ ((visibility ("default")))
int plugin_is_GPL_compatible;


#define TODO(loc, fmt, ...) warning_at(EXPR_LOCATION(loc), 0, "unitc TODO: " fmt, ##__VA_ARGS__)
#define TODO_HANDLE(t) TODO(t, "handle %s in %s:%i", get_tree_code_name(TREE_CODE(t)), __FUNCTION__, __LINE__)

// Base units.

typedef tree base_unit_t;

static base_unit_t intern_base_unit(const char *name, size_t length) {
  return get_identifier_with_length(name, length);
}

const char *base_unit_string(base_unit_t b) {
  return IDENTIFIER_POINTER(b);
}

// Units

#define MAX_UNIT_SIZE 16

typedef int16_t power_t;

struct unit {
  base_unit_t base_unit[MAX_UNIT_SIZE];
  power_t power[MAX_UNIT_SIZE];
};

static struct unit unit_one;

static const char *unit_string(const struct unit *u) {
  char result[1000];
  int p = 0;
#define P(fmt, ...) if (p < 1000) { int n = snprintf(result + p, 1000 - p, fmt, ##__VA_ARGS__); if (n > 0) p += n; }
  for (int i = 0; i < MAX_UNIT_SIZE; i++) {
    if (u->power[i] < 0) {
      for(int j = 0; j < -u->power[i]; j++) {
	P("%s%s", p == 0 ? "1 / " : " / ", base_unit_string(u->base_unit[i]));
      }
    }
    if (u->power[i] > 0) {
      for(int j = 0; j < u->power[i]; j++) {
	P("%s%s", p == 0 ? "" : " * ", base_unit_string(u->base_unit[i]));
      }
    }
  }
  if (p == 0) {
    P("1");
  }
#undef P
  return xstrdup(result);
}

static void unit_mul_assign_base_unit_power(struct unit *u, base_unit_t b, power_t p) {
  for (int i = 0; i < MAX_UNIT_SIZE; i++) {
    if (u->power[i] == 0) {
      u->base_unit[i] = b;
      u->power[i] = p;
      return;
    }
    if (u->base_unit[i] == b) {
      u->power[i] += p;
      if (u->power[i] == 0) {
	for (int j = i; j < MAX_UNIT_SIZE - 1; j++) {
  	  u->base_unit[j] = u->base_unit[j + 1];
	  u->power[j] = u->power[j + 1];
	}
	u->base_unit[MAX_UNIT_SIZE - 1] = 0;
	u->power[MAX_UNIT_SIZE - 1] = 0;
      }
      return;
    }
    if (u->base_unit[i] > b) {
      base_unit_t tmp_b = u->base_unit[i];
      power_t tmp_p = u->power[i];
      u->base_unit[i] = b;
      u->power[i] = p;
      b = tmp_b;
      p = tmp_p;
    }
  }
}

static void unit_mul_assign(struct unit *lhs, const struct unit *rhs) {
  for (int i = 0; i < MAX_UNIT_SIZE && rhs->power[i] != 0; i++) {
    unit_mul_assign_base_unit_power(lhs, rhs->base_unit[i], rhs->power[i]);
  }
}

static void unit_div_assign(struct unit *lhs, const struct unit *rhs) {
  for (int i = 0; i < MAX_UNIT_SIZE && rhs->power[i] != 0; i++) {
    unit_mul_assign_base_unit_power(lhs, rhs->base_unit[i], -rhs->power[i]);
  }
}

static struct unit unit_mul(struct unit a, struct unit b) {
  unit_mul_assign(&a, &b);
  return a;
}

static struct unit unit_div(struct unit a, struct unit b) {
  unit_div_assign(&a, &b);
  return a;
}

static void skip_white(const char **sp) {
  while (ISSPACE(**sp)) (*sp)++;
}

// Parser for units.

/*
TODO:
struct parsed_unit {
  bool ok;
  struct unit unit;
}
*/

static bool parse_factor(const char **sp, struct unit *out);
static bool parse_product(const char **sp, struct unit *out);

static bool parse_factor(const char **sp, struct unit *out) {
  if (**sp == '1' && !ISDIGIT(*(*sp + 1))) {
    (*sp)++;
    *out = unit_one;
    return true;
  }
  else if (ISALPHA(**sp)) {
    const char *start = *sp;
    // identifier
    while (ISALPHA(**sp)) {
      (*sp)++;
    }
    const char *end = *sp;
    base_unit_t base_unit = intern_base_unit(start, end - start);
    struct unit tmp = {
      .base_unit = { base_unit },
      .power = { 1 },
    };
    *out = tmp;
    return true;
  }
  else if (**sp == '(') {
    (*sp)++;
    skip_white(sp);
    if (!parse_product(sp, out)) {
      return false;
    }
    skip_white(sp);
    if (**sp != ')') {
      return false;
    }
    (*sp)++;
    return true;
  }
  return false;
}

static bool parse_product(const char **sp, struct unit *out) {
  skip_white(sp);
  if (!parse_factor(sp, out)) {
    return false;
  }
  skip_white(sp);
  while (true) {
    char op = **sp;
    if (!(op == '*' || op == '/')) {
      break;
    }
    (*sp)++;
    skip_white(sp);
    struct unit tmp;
    if (!parse_factor(sp, &tmp)) {
      return false;
    }
    if (op == '*') { unit_mul_assign(out, &tmp); }
    else if (op == '/') { unit_div_assign(out, &tmp); }
    else { internal_error("impossible"); }
  }
  skip_white(sp);
  if (**sp != '\0') {
    error(*sp);
    return false;
  }
  return true;
}

static bool parse_unit_string(const char *unit_str, struct unit *out) {
  bool ok = parse_product(&unit_str, out);
  if (!ok) {
    error("parse_unit_str");
    error(unit_str);
  }
  return ok;
}

static bool parse_unit_attribute(tree t, struct unit *out) {
  if (t == NULL_TREE) { error("null tree attribute"); return false; }
  if (TREE_CODE(t) != TREE_LIST) { error("too few attribute args"); return false; }
  if (TREE_CHAIN(t) != NULL_TREE) { error("too many attribute args"); return false; }
  tree value = TREE_VALUE(t);
  if (value == NULL_TREE) { error("null attribute value"); return false; }
  if (TREE_CODE(value) != STRING_CST) { error("attribute not a string"); return false; }
  const char *str = TREE_STRING_POINTER(value);
  return parse_unit_string(str, out);
}

// Unit checking.

struct maybe_unit {
  bool has_unit;
  struct unit unit;
};

static struct maybe_unit no_unit;

static struct maybe_unit just_one = {
  .has_unit = true,
  .unit = {},
};

struct maybe_unit some_unit(struct unit unit) {
  struct maybe_unit m = {
    .has_unit = true,
    .unit = unit,
  };
  return m;
}

static struct maybe_unit check_multiplication(struct maybe_unit a,
					      struct maybe_unit b,
					      tree loc)
{
  (void) loc; // multiplication always succeeds
  return a.has_unit && b.has_unit ? some_unit(unit_mul(a.unit, b.unit)) : no_unit;
}

static struct maybe_unit check_division(struct maybe_unit a,
					      struct maybe_unit b,
					      tree loc)
{
  (void) loc; // division always succeeds
  return a.has_unit && b.has_unit ? some_unit(unit_div(a.unit, b.unit)) : no_unit;
}

static struct maybe_unit check_assignment(const char *operation_type, struct maybe_unit a, struct maybe_unit b, tree loc) {
  if (a.has_unit && b.has_unit) {
    unit c = unit_div(a.unit, b.unit);
    if (!c.power[0] == 0) {
      error_at(EXPR_LOCATION(loc), "%s from unit `%s' to unit `%s'", operation_type, unit_string(&b.unit), unit_string(&a.unit)); // TODO free
    }
  }
  return a;
}

static struct maybe_unit check_comparison(struct maybe_unit a, struct maybe_unit b, tree loc) {
  if (a.has_unit &&  b.has_unit) {
    TODO(loc,"check that units are the same in comparison: `%s' and `%s'", unit_string(&a.unit), unit_string(&b.unit));
    debug_generic_expr(loc);
    (void) loc;
  }
  return just_one;
}

static struct maybe_unit check_attributes(tree attrs, tree loc) {
  tree unit_attribute = lookup_attribute("unit", attrs);
  if (unit_attribute == NULL_TREE) { return no_unit; }
  struct unit unit;
  if (!parse_unit_attribute(TREE_VALUE(unit_attribute), &unit)) {
    error_at(EXPR_LOCATION(loc), "malformed unit attribute");
    return no_unit;
  }
  return some_unit(unit);
}

static struct maybe_unit check_decl(tree decl) {
  struct maybe_unit type_unit = check_attributes(TYPE_ATTRIBUTES(TREE_TYPE(decl)), decl);
  struct maybe_unit decl_unit = check_attributes(DECL_ATTRIBUTES(decl), decl);
  struct maybe_unit func_unit = TREE_CODE(decl) == RESULT_DECL ? check_attributes(DECL_ATTRIBUTES(DECL_CONTEXT(decl)), decl) : no_unit;
  struct maybe_unit result = just_one;
  int count = 0;
  if (type_unit.has_unit) { count++; result = type_unit; }
  if (decl_unit.has_unit) { count++; result = decl_unit; }
  if (func_unit.has_unit) { count++; result = func_unit; }
  if (count > 1) { error("too many unit attributes"); return no_unit; }
  return result;
}

static struct maybe_unit check(tree t) {
  if (t == NULL_TREE) {
    fprintf(stderr, "check: null tree\n");
    return no_unit;
  }
  switch (TREE_CODE(t)) {
  case INTEGER_CST:
    return just_one;
  case REAL_CST:
    return just_one;
  case PARM_DECL:
    return check_decl(t);
  case RESULT_DECL:
    return check_decl(t);
  case RETURN_EXPR:
    return check(TREE_OPERAND(t, 0));
  case DECL_EXPR:
    check(DECL_EXPR_DECL(t));
    return no_unit;
  case VAR_DECL:
    return check_assignment("initialization assignment", check_decl(t), DECL_INITIAL(t) ? check(DECL_INITIAL(t)) : no_unit, t);
  case MODIFY_EXPR:
    return check_assignment("assignment",
			    check(TREE_OPERAND(t, 0)),
			    check(TREE_OPERAND(t, 1)),
			    t);
  case MULT_EXPR:
    return check_multiplication(check(TREE_OPERAND(t, 0)),
				check(TREE_OPERAND(t, 1)),
				t);
  case RDIV_EXPR:
    return check_division(check(TREE_OPERAND(t, 0)),
			  check(TREE_OPERAND(t, 1)),
			  t);
  case EQ_EXPR:
  case NE_EXPR:
  case GE_EXPR:
  case LE_EXPR:
  case GT_EXPR:
  case LT_EXPR:
    return check_comparison(check(TREE_OPERAND(t, 0)),
			    check(TREE_OPERAND(t, 1)),
			    t);
  case BIND_EXPR:
    (void) BIND_EXPR_VARS(t);
    check(BIND_EXPR_BODY(t));
    return no_unit;
  case STATEMENT_LIST:
    {
      struct maybe_unit last = no_unit;
      for (tree_stmt_iterator iter = tsi_start(t); !tsi_end_p(iter); tsi_next(&iter)) {
	last = check(tsi_stmt(iter));
      }
      return last;
    }
  case FLOAT_EXPR:
  case NOP_EXPR:
    return check_assignment("cast",
			    check_attributes(TYPE_ATTRIBUTES(TREE_TYPE(t)), t),
			    check(TREE_OPERAND(t, 0)),
			    t);
  default:
    TODO_HANDLE(t);
    return no_unit;
  }
}

// GCC plugin callbacks.

static void handle_finish_type(void *gcc_data, void *user_data)
{
  (void) user_data;
  tree t = (tree) gcc_data;
  tree attrs = TYPE_ATTRIBUTES(t);
  (void) attrs;

  //fprintf(stderr, "finish_type: %s\n", get_tree_code_name(TREE_CODE(t)));
  //debug_generic_expr(t);
  //debug_tree_chain(attrs);
}

static void handle_finish_decl(void *gcc_data, void *user_data)
{
  (void) user_data;
  tree decl = (tree) gcc_data;
  check_decl(decl);
}

static void handle_finish_function(void *gcc_data, void *user_data) {
  (void) user_data;
  tree func = (tree) gcc_data;
  tree body = DECL_SAVED_TREE(func);
  (void) body;
  //check(body);
}

static void handle_pre_genericize(void *gcc_data, void *user_data) {
  tree func = (tree) gcc_data;
  (void) user_data;
  tree body = DECL_SAVED_TREE(func);
  (void) body;
  check(body);
}

  // GCC plugin initialization.

const struct attribute_spec unit_attribute_spec = {
  .name = "unit",
  .min_length = 1,
  .max_length = 1,
  .decl_required = false,
  .type_required = false,
  .function_type_required = false,
  .affects_type_identity = true,
  .handler = NULL,
  .exclude = NULL,
};

static void register_attributes(void *gcc_data, void *user_data)
{
  (void) gcc_data;
  (void) user_data;
  register_attribute(&unit_attribute_spec);
}


__attribute__ ((visibility ("default")))
int plugin_init(struct plugin_name_args *plugin_info,
	    struct plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) {
    return 1;
  }
  const char *plugin_name = plugin_info->base_name;
  struct plugin_info pi = { "0.1", "UnitC" };
  register_callback(plugin_name, PLUGIN_INFO, NULL, &pi);
  register_callback(plugin_name, PLUGIN_ATTRIBUTES, &register_attributes, NULL);
  register_callback(plugin_name, PLUGIN_FINISH_TYPE, &handle_finish_type, NULL);
  register_callback(plugin_name, PLUGIN_FINISH_DECL, &handle_finish_decl, NULL);
  register_callback(plugin_name, PLUGIN_FINISH_PARSE_FUNCTION, &handle_finish_function, NULL);
  register_callback(plugin_name, PLUGIN_PRE_GENERICIZE, &handle_pre_genericize, NULL);
  return 0;
}

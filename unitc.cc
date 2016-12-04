#include <stdlib.h>
#include <gcc-plugin.h>
#include <plugin-version.h>
#include <tree.h>
#include <tree-iterator.h>
#include <tree-pretty-print.h>
#include <diagnostic.h>
#include <stringpool.h>

#include <assert.h>

__attribute__ ((visibility ("default")))
int plugin_is_GPL_compatible;


#define TODO(fmt, ...) warning(0, "unitc TODO: " fmt, ##__VA_ARGS__)
#define TODO_HANDLE(t) TODO("handle %s in %s:%i", get_tree_code_name(TREE_CODE(t)), __FUNCTION__, __LINE__)


static void handle_finish_type(void *gcc_data, void *user_data)
{
  (void) user_data;
  tree t = (tree) gcc_data;
  tree attrs = TYPE_ATTRIBUTES(t);

  fprintf(stderr, "finish_type: %s\n",
	  get_tree_code_name(TREE_CODE(t)));
  debug_generic_expr(t);
  debug_tree_chain(attrs);
}

static void handle_finish_decl(void *gcc_data, void *user_data)
{
  (void) user_data;
  tree t = (tree) gcc_data;
  tree type = TREE_TYPE(t);
  tree attrs = TYPE_ATTRIBUTES(type);
  tree unit_attr = lookup_attribute("unit", attrs);

  fprintf(stderr, "finish_decl: %s %s\n",
	  get_tree_code_name(TREE_CODE(t)),
	  get_tree_code_name(TREE_CODE(type)));

  fprintf(stderr, "t: ");
  debug_generic_expr(t);
  fprintf(stderr, "type: ");
  debug_generic_expr(type);
  //fprintf(stderr, "attrs: ");
  //debug_tree_chain(attrs);
  if (unit_attr != NULL_TREE) {
    fprintf(stderr, "unit: ");
    debug_generic_expr(unit_attr);
  }
}

static void check(tree t) {
  switch (TREE_CODE(t)) {
  case INTEGER_CST:
    break;
  case REAL_CST:
    break;
  case PARM_DECL:
    break;
  case RESULT_DECL:
    break;
  case RETURN_EXPR:
    check(TREE_OPERAND(t, 0));
    break;
  case DECL_EXPR:
    check(DECL_EXPR_DECL(t));
    break;
  case VAR_DECL:
    check(DECL_INITIAL(t));
    break;
  case MODIFY_EXPR:
    //TODO("check MODIFY_EXPR");
    debug_generic_expr(t);
    check(TREE_OPERAND(t, 0));
    check(TREE_OPERAND(t, 1));
    break;
  case MULT_EXPR:
    check(TREE_OPERAND(t, 0));
    check(TREE_OPERAND(t, 1));
    break;
  case GT_EXPR:
    check(TREE_OPERAND(t, 0));
    check(TREE_OPERAND(t, 1));
    break;
  case BIND_EXPR:
    (void) BIND_EXPR_VARS(t);
    check(BIND_EXPR_BODY(t));
    break;
  case STATEMENT_LIST:
    for (tree_stmt_iterator iter = tsi_start(t); !tsi_end_p(iter); tsi_next(&iter)) {
      fprintf(stderr, "iter: ");
      check(tsi_stmt(iter));
    }
    break;
  default:
    TODO_HANDLE(t);
    debug_generic_expr(t);
    break;
  }
}

typedef tree base_unit_t;
static base_unit_t intern_base_unit(const char *name, size_t length) {
  return get_identifier_with_length(name, length);
}

const char *base_unit_string(base_unit_t b) {
  return IDENTIFIER_POINTER(b);
}


#define MAX_UNIT_SIZE 16
typedef int16_t power_t;

struct unit {
  base_unit_t base_unit[MAX_UNIT_SIZE];
  power_t power[MAX_UNIT_SIZE];
};

static void dump_unit(const struct unit *u) {
  bool anything_printed = false;
  for (int i = 0; i < MAX_UNIT_SIZE; i++) {
    if (u->power[i] != 0) {
      anything_printed = true;
      fprintf(stderr, base_unit_string(u->base_unit[i]));
      if (u->power[i] != 1) {
	fprintf(stderr, "^%i", u->power[i]);
      }			    
    }
  }
  if (!anything_printed) {
    fprintf(stderr, "1");
  }
  fprintf(stderr, "\n");
}

static struct unit unit_one;

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

static void handle_finish_function(void *gcc_data, void *user_data) {
  (void) user_data;
  tree t = (tree) gcc_data;
  tree args = DECL_ARGUMENTS(t);
  tree type = TREE_TYPE(t);
  tree body = DECL_SAVED_TREE(t);
  (void) args;
  (void) type;
  check(body);
}

static void skip_white(const char **sp) {
  while (ISSPACE(**sp)) (*sp)++;
}

// Parser for units.
// Right now only checks syntax.
static bool parse_factor(const char **sp, struct unit *out);
static bool parse_product(const char **sp, struct unit *out);

static bool parse_factor(const char **sp, struct unit *out) {
  if (**sp == '1' && !ISDIGIT(*(*sp + 1))) {
    // dimensionless unit
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
    return false;
  }
  return true;
}

static bool parse_unit_string(const char *unit_str, struct unit *out) {
  return parse_product(&unit_str, out);
}

static bool parse_unit_attribute(tree t, struct unit *out) {
  return
    t != NULL_TREE &&
    TREE_CODE(t) == TREE_LIST &&
    TREE_CHAIN(t) == NULL_TREE &&
    TREE_VALUE(t) != NULL_TREE &&
    TREE_CODE(TREE_VALUE(t)) == STRING_CST &&
    parse_unit_string(TREE_STRING_POINTER(TREE_VALUE(t)), out);
}

static tree handle_unit_attribute(tree *node, tree name, tree args,
				  int flags, bool *no_add_attrs) {
  (void) node;
  (void) name;
  (void) flags;
  (void) no_add_attrs;
  struct unit u;
  if (!parse_unit_attribute(args, &u)) {
    error("unit not well-formed");
  } else {
    dump_unit(&u);
  }
  return NULL_TREE;
}

const struct attribute_spec unit_attribute_spec = {
  .name = "unit",
  .min_length = 1,
  .max_length = 1,
  .decl_required = false,
  .type_required = true,
  .function_type_required = false,
  .handler = &handle_unit_attribute,
  .affects_type_identity = false,
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
  return 0;
}

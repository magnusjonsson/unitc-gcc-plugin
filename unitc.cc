#include <gcc-plugin.h>
#include <plugin-version.h>
#include <tree.h>
#include <tree-iterator.h>
#include <tree-pretty-print.h>

__attribute__ ((visibility ("default"))) int plugin_is_GPL_compatible;

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

static void handle_finish_function(void *gcc_data, void *user_data) {
  (void) user_data;
  tree t = (tree) gcc_data;
  tree args = DECL_ARGUMENTS(t);
  tree type = TREE_TYPE(t);
  tree saved_tree = DECL_SAVED_TREE(t);

  fprintf(stderr, "finish_function: %s %s %s %s\n",
	  get_tree_code_name(TREE_CODE(t)),
	  get_tree_code_name(TREE_CODE(type)),
	  get_tree_code_name(TREE_CODE(args)),
	  get_tree_code_name(TREE_CODE(saved_tree)));
	 
  debug_generic_expr(t);
  debug_generic_expr(type);
  debug_tree_chain(args);
  debug_generic_expr(saved_tree);

  // this is not working
  for (tree_stmt_iterator tsi = tsi_start(saved_tree); tsi_end_p(tsi); tsi_next(&tsi)) {
    fprintf(stderr, "tsi: ");
    debug_generic_expr(tsi_stmt(tsi));
  }
}


static tree handle_unit_attribute(tree *node, tree name, tree args,
				  int flags, bool *no_add_attrs) {
  (void) name;
  (void) flags;
  (void) no_add_attrs;
  fprintf(stderr, "handle_unit_attribute: %s %i %s\n",
	  get_tree_code_name(TREE_CODE(*node)),
	  flags,
	  get_tree_code_name(TREE_CODE(args)));
  //debug_generic_expr(*node);
  //debug_tree_chain(args);
  // TODO validate attribute
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


__attribute__ ((visibility ("default"))) int plugin_init(struct plugin_name_args *plugin_info,
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

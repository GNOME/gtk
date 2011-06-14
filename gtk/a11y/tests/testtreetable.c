#include <stdlib.h>
#include <testlib.h>

/*
 * This module is for use with the test program testtreeview
 */
static gboolean state_change_watch  (GSignalInvocationHint *ihint,
                                     guint                  n_param_values,
                                     const GValue          *param_values,
                                     gpointer               data);

static void _check_table             (AtkObject         *in_obj);
static void _check_cell_actions      (AtkObject         *in_obj);

static gint _find_expander_column    (AtkTable          *table);
static void _check_expanders         (AtkTable          *table,
                                      gint              expander_column);
static void _runtest                 (AtkObject         *obj);
static void _create_event_watcher    (void);
static void row_inserted             (AtkObject         *obj,
                                      gint              row,
                                      gint              count);
static void row_deleted              (AtkObject         *obj,
                                      gint              row,
                                      gint              count);
static AtkObject *table_obj = NULL;
static gint expander_column = -1;
static gboolean editing_cell = FALSE;

static gboolean 
state_change_watch (GSignalInvocationHint *ihint,
                    guint                  n_param_values,
                    const GValue          *param_values,
                    gpointer               data)
{
  GObject *object;
  gboolean state_set;
  const gchar *state_name;
  AtkStateType state_type;

  object = g_value_get_object (param_values + 0);
  g_return_val_if_fail (ATK_IS_OBJECT (object), FALSE);

  state_name = g_value_get_string (param_values + 1);
  state_set = g_value_get_boolean (param_values + 2);

  g_print ("Object type: %s state: %s set: %d\n", 
           g_type_name (G_OBJECT_TYPE (object)), state_name, state_set);
  state_type = atk_state_type_for_name (state_name);
  if (state_type == ATK_STATE_EXPANDED)
    {
      AtkObject *parent = atk_object_get_parent (ATK_OBJECT (object));

      if (ATK_IS_TABLE (parent))
        _check_expanders (ATK_TABLE (parent), expander_column);
    }
  return TRUE;
}

static void 
_check_table (AtkObject *in_obj)
{
  AtkObject *obj;
  AtkRole role[2];
  AtkRole obj_role;
  static gboolean emission_hook_added = FALSE;

  if (!emission_hook_added)
    {
      emission_hook_added = TRUE;
      g_signal_add_emission_hook (
                    g_signal_lookup ("state-change", ATK_TYPE_OBJECT),
      /*
       * To specify an emission hook for a particular state, e.g. 
       * ATK_STATE_EXPANDED, instead of 0 use
       * g_quark_from_string (atk_state_type_get_name (ATK_STATE_EXPANDED))
       */  
                    0,
                    state_change_watch, NULL, (GDestroyNotify) NULL);
    }

  role[0] = ATK_ROLE_TABLE;
  role[1] = ATK_ROLE_TREE_TABLE;
  
  g_print ("focus event for: %s\n", g_type_name (G_OBJECT_TYPE (in_obj)));

  _check_cell_actions (in_obj);

  obj_role = atk_object_get_role (in_obj);
  if (obj_role == ATK_ROLE_TABLE_CELL)
    obj = atk_object_get_parent (in_obj);
  else
    obj = find_object_by_role (in_obj, role, 2);
  if (obj == NULL)
    return;

  editing_cell = FALSE;

  if (obj != table_obj)
    {
      table_obj = obj;
      g_print("Found %s table\n", g_type_name (G_OBJECT_TYPE (obj)));
      g_signal_connect (G_OBJECT (obj),
                        "row_inserted",
                        G_CALLBACK (row_inserted),
                        NULL);
      g_signal_connect (G_OBJECT (obj),
                        "row_deleted",
                        G_CALLBACK (row_deleted),
                        NULL);
      /*
       * Find expander column
       */
      if (ATK_IS_TABLE (obj))
        {
          if (atk_object_get_role (obj) == ATK_ROLE_TREE_TABLE)
            {
              expander_column = _find_expander_column (ATK_TABLE (obj));
              if (expander_column == -1)
                {
                  g_warning ("Expander column not found\n");
                  return;
                }
            }
          _runtest(obj);
        }
      else
        g_warning ("Table does not support AtkTable interface\n");
    }
  else 
    {
      /*
       * We do not call these functions at the same time as we set the 
       * signals as the GtkTreeView may not be displayed yet.
       */
      gint x, y, width, height, first_x, last_x, last_y;
      gint first_row, last_row, first_column, last_column;
      gint index;
      AtkObject *first_child, *last_child, *header;
      AtkComponent *component = ATK_COMPONENT (obj);
      AtkTable *table = ATK_TABLE (obj);

      atk_component_get_extents (component, 
                                 &x, &y, &width, &height, ATK_XY_WINDOW);
      g_print ("WINDOW: x: %d y: %d width: %d height: %d\n",
               x, y, width, height);
      atk_component_get_extents (component, 
                                 &x, &y, &width, &height, ATK_XY_SCREEN);
      g_print ("SCREEN: x: %d y: %d width: %d height: %d\n",
               x, y, width, height);
      last_x = x + width - 1;
      last_y = y + height - 1;
      first_x = x; 
      header = atk_table_get_column_header (table, 0);
      if (header)
        {
          atk_component_get_extents (ATK_COMPONENT (header), 
                                     &x, &y, &width, &height, ATK_XY_SCREEN);
          g_print ("Got column header: x: %d y: %d width: %d height: %d\n",
                   x, y, width, height);
          y += height;
        }
      first_child = atk_component_ref_accessible_at_point (component, 
                                             first_x, y, ATK_XY_SCREEN);
      atk_component_get_extents (ATK_COMPONENT (first_child), 
                                 &x, &y, &width, &height, ATK_XY_SCREEN);
      g_print ("first_child: x: %d y: %d width: %d height: %d\n",
               x, y, width, height);
      index = atk_object_get_index_in_parent (ATK_OBJECT (first_child));
      first_row = atk_table_get_row_at_index (table, index);
      first_column = atk_table_get_column_at_index (table, index);
      g_print ("first_row: %d first_column: %d index: %d\n",
               first_row, first_column, index);
      g_assert (index == atk_table_get_index_at (table, first_row, first_column));
      last_child = atk_component_ref_accessible_at_point (component, 
                                     last_x, last_y, ATK_XY_SCREEN);
      if (last_child == NULL)
        {
          /* The TreeView may be bigger than the data */
          gint n_children;

          n_children = atk_object_get_n_accessible_children (obj);
          last_child = atk_object_ref_accessible_child (obj, n_children - 1);
        }
      atk_component_get_extents (ATK_COMPONENT (last_child), 
                                 &x, &y, &width, &height, ATK_XY_SCREEN);
      g_print ("last_child: x: %d y: %d width: %d height: %d\n",
               x, y, width, height);
      index = atk_object_get_index_in_parent (ATK_OBJECT (last_child));
      last_row = atk_table_get_row_at_index (table, index);
      last_column = atk_table_get_column_at_index (table, index);
      g_print ("last_row: %d last_column: %d index: %d\n",
               last_row, last_column, index);
      g_assert (index == atk_table_get_index_at (table, last_row, last_column));
      g_object_unref (first_child);
      g_object_unref (last_child);

      if (expander_column >= 0)
        {
          gint n_rows, i;
          gint x, y, width, height;

          n_rows = atk_table_get_n_rows (table);
          for (i = 0; i < n_rows; i++)
            {
              AtkObject *child_obj;
              AtkStateSet *state_set;
              gboolean showing;

              child_obj = atk_table_ref_at (table, i, expander_column);
              state_set = atk_object_ref_state_set (child_obj);
              showing =  atk_state_set_contains_state (state_set, 
                                                       ATK_STATE_SHOWING);
              g_object_unref (state_set);
              atk_component_get_extents (ATK_COMPONENT (child_obj), 
                                         &x, &y, &width, &height, 
                                         ATK_XY_SCREEN);
              g_object_unref (child_obj);
              if (showing)
                g_print ("Row: %d Column: %d x: %d y: %d width: %d height: %d\n", 
                         i, expander_column, x, y, width, height);
            }
        }
    }
}

static void 
_check_cell_actions (AtkObject *in_obj)
{
  AtkRole role;
  AtkAction *action;
  gint n_actions, i;

  role = atk_object_get_role (in_obj);
  if (role != ATK_ROLE_TABLE_CELL)
    return;

  if (!ATK_IS_ACTION (in_obj))
    return;

  if (editing_cell)
    return;

  action = ATK_ACTION (in_obj);

  n_actions = atk_action_get_n_actions (action);

  for (i = 0; i < n_actions; i++)
    {
      const gchar* name;

      name = atk_action_get_name (action, i);
      g_print ("Action %d is %s\n", i, name);
#if 0
      if (strcmp (name, "edit") == 0)
        {
          editing_cell = TRUE;
          atk_action_do_action (action, i);
        }
#endif
    }
  return;
}

static gint 
_find_expander_column (AtkTable *table)
{
  gint n_columns, i;
  gint retval = -1;

  n_columns = atk_table_get_n_columns (table);
  for (i = 0; i < n_columns; i++)
    {
      AtkObject *cell;
      AtkRelationSet *relation_set;

      cell = atk_table_ref_at (table, 0, i);
      relation_set =  atk_object_ref_relation_set (cell);
      if (atk_relation_set_contains (relation_set, 
                                     ATK_RELATION_NODE_CHILD_OF))
        retval = i;
      g_object_unref (relation_set);
      g_object_unref (cell);
      if (retval >= 0)
        break;
    }
  return retval;
}

static void
_check_expanders (AtkTable *table,
                  gint     expander_column)
{
  gint n_rows, i;

  n_rows = atk_table_get_n_rows (table);

  for (i = 0; i < n_rows; i++)
    {
      AtkObject *cell;
      AtkRelationSet *relation_set;
      AtkRelation *relation;
      GPtrArray *target;
      gint j;

      cell = atk_table_ref_at (table, i, expander_column);

      relation_set =  atk_object_ref_relation_set (cell);
      relation = atk_relation_set_get_relation_by_type (relation_set, 
                                     ATK_RELATION_NODE_CHILD_OF);
      g_assert (relation);
      target = atk_relation_get_target (relation);
      g_assert (target->len == 1);
      for (j = 0; j < target->len; j++)
        {
          AtkObject *target_obj;
          AtkRole role;
          gint target_index, target_row;

          target_obj = g_ptr_array_index (target, j);
          role = atk_object_get_role (target_obj);

          switch (role)
            {
            case ATK_ROLE_TREE_TABLE:
              g_print ("Row %d is top level\n", i);
              break;
            case ATK_ROLE_TABLE_CELL:
              target_index = atk_object_get_index_in_parent (target_obj);
              target_row = atk_table_get_row_at_index (table, target_index);
              g_print ("Row %d has parent at %d\n", i, target_row);
              break;
            default:
              g_assert_not_reached ();
            } 
        }
      g_object_unref (relation_set);
      g_object_unref (cell);
    }
}

static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_check_table);
}

int
gtk_module_init (gint argc, 
                 char *argv[])
{
  g_print ("testtreetable Module loaded\n");

  _create_event_watcher ();

  return 0;
}

static void
_runtest (AtkObject *obj)
{
  AtkObject *child_obj;
  AtkTable *table;
  AtkObject *caption;
  gint i;
  gint n_cols, n_rows, n_children; 

  table = ATK_TABLE (obj);
  n_children = atk_object_get_n_accessible_children (ATK_OBJECT (obj));
  n_cols = atk_table_get_n_columns (table);
  n_rows = atk_table_get_n_rows (table);
  g_print ("n_children: %d n_rows: %d n_cols: %d\n", 
            n_children, n_rows, n_cols);
  
  for (i = 0; i < n_rows; i++)
    {
      gint index = atk_table_get_index_at (table, i, expander_column);
      gint index_in_parent;

      child_obj = atk_table_ref_at (table, i, expander_column);
      index_in_parent = atk_object_get_index_in_parent (child_obj);
      g_print ("index: %d %d row %d column %d\n", index, index_in_parent, i, expander_column);
      
      g_object_unref (child_obj);
    }
  caption = atk_table_get_caption (table);
  if (caption)
    {
      const gchar *caption_name = atk_object_get_name (caption);

      g_print ("Caption: %s\n", caption_name ? caption_name : "<null>");
    }
  for (i = 0; i < n_cols; i++)
    {
      AtkObject *header;

      header = atk_table_get_column_header (table, i);
      g_print ("Header for column %d is %p\n", i, header);
      if (header)
        {
	  const gchar *name;
	  AtkRole role;
          AtkObject *parent;
          AtkObject *child;
          gint index;

          name = atk_object_get_name (header);
          role = atk_object_get_role (header);
          parent = atk_object_get_parent (header);

          if (parent)
            {
              index = atk_object_get_index_in_parent (header);
              g_print ("Parent: %s index: %d\n", G_OBJECT_TYPE_NAME (parent), index);
              child = atk_object_ref_accessible_child (parent, 0);
              g_print ("Child: %s %p\n", G_OBJECT_TYPE_NAME (child), child);
              if (index >= 0)
                {
                  child = atk_object_ref_accessible_child (parent, index);
                  g_print ("Index: %d child: %s\n", index, G_OBJECT_TYPE_NAME (child));
                  g_object_unref (child);
                }
            }
          else
            g_print ("Parent of header is NULL\n");
          g_print ("%s %s %s\n", G_OBJECT_TYPE_NAME (header), name ? name: "<null>", atk_role_get_name (role));
        }
    }
}

static void 
row_inserted (AtkObject *obj,
              gint      row,
              gint      count)
{
#if 0
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
#endif
  gint index;

  g_print ("row_inserted: row: %d count: %d\n", row, count);
  index = atk_table_get_index_at (ATK_TABLE (obj), row+1, 0);
  g_print ("index for first column of next row is %d\n", index);

#if 0
  widget = GTK_ACCESSIBLE (obj)->widget;
  tree_view = GTK_TREE_VIEW (widget);
  tree_model = gtk_tree_view_get_model (tree_view);
  if (GTK_IS_TREE_STORE (tree_model))
    {
      GtkTreeStore *tree_store;
      GtkTreePath *tree_path;
      GtkTreeIter tree_iter;

      tree_store = GTK_TREE_STORE (tree_model);
      tree_path = gtk_tree_path_new_from_string ("3:0");
      gtk_tree_model_get_iter (tree_model, &tree_iter, tree_path);
      gtk_tree_path_free (tree_path);
      gtk_tree_store_remove (tree_store, &tree_iter);
    }
#endif
}

static void 
row_deleted (AtkObject *obj,
             gint      row,
             gint      count)
{
#if 0
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
#endif
  gint index;

  g_print ("row_deleted: row: %d count: %d\n", row, count);
  index = atk_table_get_index_at (ATK_TABLE (obj), row+1, 0);
  g_print ("index for first column of next row is %d\n", index);

#if 0
  widget = GTK_ACCESSIBLE (obj)->widget;
  tree_view = GTK_TREE_VIEW (widget);
  tree_model = gtk_tree_view_get_model (tree_view);
  if (GTK_IS_TREE_STORE (tree_model))
    {
      GtkTreeStore *tree_store;
      GtkTreePath *tree_path;
      GtkTreeIter tree_iter, new_iter;

      tree_store = GTK_TREE_STORE (tree_model);
      tree_path = gtk_tree_path_new_from_string ("2");
      gtk_tree_model_get_iter (tree_model, &tree_iter, tree_path);
      gtk_tree_path_free (tree_path);
      gtk_tree_store_insert_before (tree_store, &new_iter, NULL, &tree_iter);
    }
#endif
}

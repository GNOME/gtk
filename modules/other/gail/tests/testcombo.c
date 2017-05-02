#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>
#include "testlib.h"

static void _test_selection (AtkObject *obj);
static void _check_combo_box (AtkObject *obj);
static void _check_children (AtkObject *obj);
static gint _open_combo_list (gpointer data);
static gint _close_combo_list (gpointer data);

#define NUM_VALID_ROLES 1

static void _check_children (AtkObject *obj)
{
  gint n_children, i, j;
  AtkObject *child;
  AtkObject *grand_child;
  AtkObject *parent;

  n_children = atk_object_get_n_accessible_children (obj);

  if (n_children > 1)
  {
    g_print ("*** Unexpected number of children for combo box: %d\n", 
             n_children);
    return;
  }
  if (n_children == 2)
  {
    child = atk_object_ref_accessible_child (obj, 1);
    g_return_if_fail (atk_object_get_role (child) == ATK_ROLE_TEXT);
    parent = atk_object_get_parent (child);
    j = atk_object_get_index_in_parent (child);
    if (j != 1)
     g_print ("*** inconsistency between parent and children %d %d ***\n",
              1, j);       
    g_object_unref (G_OBJECT (child));
  }

  child = atk_object_ref_accessible_child (obj, 0);
  g_return_if_fail (atk_object_get_role (child) == ATK_ROLE_LIST);
  parent = atk_object_get_parent (child);
  j = atk_object_get_index_in_parent (child);
  if (j != 0)
     g_print ("*** inconsistency between parent and children %d %d ***\n",
              0, j);       

  n_children = atk_object_get_n_accessible_children (child);
  for (i = 0; i < n_children; i++)
  {
    const gchar *name;

    grand_child = atk_object_ref_accessible_child (child, i);
    name = atk_object_get_name (grand_child);
    g_print ("Index: %d Name: %s\n", i, name ? name : "<NULL>");
    g_object_unref (G_OBJECT (grand_child));
  }
  g_object_unref (G_OBJECT (child));
}
  
static void _test_selection (AtkObject *obj)
{
  gint count;
  gint n_children;
  AtkObject *list;

  count = atk_selection_get_selection_count (ATK_SELECTION (obj));
  g_return_if_fail (count == 0);

  list = atk_object_ref_accessible_child (obj, 0);
  n_children = atk_object_get_n_accessible_children (list); 
  g_object_unref (G_OBJECT (list));

  atk_selection_add_selection (ATK_SELECTION (obj), n_children - 1);
  count = atk_selection_get_selection_count (ATK_SELECTION (obj));
  g_return_if_fail (count == 1);
  g_return_if_fail (atk_selection_is_child_selected (ATK_SELECTION (obj),
                     n_children - 1));	
  atk_selection_add_selection (ATK_SELECTION (obj), 0);
  count = atk_selection_get_selection_count (ATK_SELECTION (obj));
  g_return_if_fail (count == 1);
  g_return_if_fail (atk_selection_is_child_selected (ATK_SELECTION (obj), 0));
  atk_selection_clear_selection (ATK_SELECTION (obj));
  count = atk_selection_get_selection_count (ATK_SELECTION (obj));
  g_return_if_fail (count == 0);
}

static void _check_combo_box (AtkObject *obj)
{
  static gboolean done = FALSE;
  static gboolean done_selection = FALSE;
  AtkRole role;

  role = atk_object_get_role (obj);

  if (role == ATK_ROLE_FRAME)
  {
    AtkRole roles[NUM_VALID_ROLES];
    AtkObject *combo_obj;

    if (done_selection)
      return;

    roles[0] = ATK_ROLE_COMBO_BOX;

    combo_obj = find_object_by_role (obj, roles, NUM_VALID_ROLES);

    if (combo_obj)
    {
      if (!done_selection)
      {
        done_selection = TRUE;
      }
      if (g_getenv ("TEST_ACCESSIBLE_COMBO_NOEDIT") != NULL)
      {
        GtkEntry *entry;

        entry = GTK_ENTRY (GTK_COMBO (GTK_ACCESSIBLE (combo_obj)->widget)->entry);
        gtk_entry_set_editable (entry, FALSE);
      }
      _check_children (combo_obj);
      _test_selection (combo_obj);
    }

    return;
  }
  if (role != ATK_ROLE_COMBO_BOX)
    return;

  g_print ("*** Start ComboBox ***\n");
  _check_children (obj);
 
  if (!done)
  {
    gtk_idle_add (_open_combo_list, obj);
    done = TRUE;
  }
  else
      return;
  g_print ("*** End ComboBox ***\n");
}

static gint _open_combo_list (gpointer data)
{
  AtkObject *obj = ATK_OBJECT (data);

  g_print ("_open_combo_list\n");
  atk_action_do_action (ATK_ACTION (obj), 0);

  gtk_timeout_add (5000, _close_combo_list, obj);
  return FALSE;
}

static gint _close_combo_list (gpointer data)
{
  AtkObject *obj = ATK_OBJECT (data);

  gint count;
  gint n_children;
  AtkObject *list;

  count = atk_selection_get_selection_count (ATK_SELECTION (obj));
  g_return_val_if_fail (count == 0, FALSE);

  list = atk_object_ref_accessible_child (obj, 0);
  n_children = atk_object_get_n_accessible_children (list); 
  g_object_unref (G_OBJECT (list));

  atk_selection_add_selection (ATK_SELECTION (obj), n_children - 1);

  atk_action_do_action (ATK_ACTION (obj), 0);

  return FALSE;
}

static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_check_combo_box);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("testcombo Module loaded\n");

  _create_event_watcher();

  return 0;
}

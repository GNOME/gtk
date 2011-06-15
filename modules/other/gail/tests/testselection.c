#include <string.h>
#include <atk/atk.h>
#include <gtk/gtk.h>

/*
 * This module tests the selection interface on menu items.
 * To use this module run the test program testgtk and use the menus 
 * option.
 */
static void _do_selection (AtkObject *obj);
static gint _finish_selection (gpointer data);
static AtkObject* _find_object (AtkObject* obj, AtkRole role);
static void _print_type (AtkObject *obj);

static AtkObject* 
_find_object (AtkObject *obj, 
              AtkRole   role)
{
  /*
   * Find the first object which is a descendant of the specified object
   * which matches the specified role.
   *
   * This function returns a reference to the AtkObject which should be
   * removed when finished with the object.
   */
  gint i;
  gint n_children;
  AtkObject *child;

  n_children = atk_object_get_n_accessible_children (obj);
  for (i = 0; i < n_children; i++) 
  {
    AtkObject* found_obj;

    child = atk_object_ref_accessible_child (obj, i);
    if (atk_object_get_role (child) == role)
    {
      return child;
    }
    found_obj = _find_object (child, role);
    g_object_unref (child);
    if (found_obj)
    {
      return found_obj;
    }
  }
  return NULL;
}

static void _print_type (AtkObject *obj)
{
  const gchar * typename = NULL;
  const gchar * name = NULL;
  AtkRole role;

  if (GTK_IS_ACCESSIBLE (obj))
  {
    GtkWidget* widget = NULL;

    widget = GTK_ACCESSIBLE (obj)->widget;
    typename = g_type_name (G_OBJECT_TYPE (widget));
    g_print ("Widget type name: %s\n", typename ? typename : "NULL");
  }
  typename = g_type_name (G_OBJECT_TYPE (obj));
  g_print ("Accessible type name: %s\n", typename ? typename : "NULL");
  name = atk_object_get_name (obj);
  g_print("Accessible Name: %s\n", (name) ? name : "NULL");
  role = atk_object_get_role(obj);
  g_print ("Accessible Role: %d\n", role);
}

static void 
_do_selection (AtkObject *obj)
{
  gint i;
  AtkObject *selected;
  AtkRole role;
  AtkObject *selection_obj;

  role = atk_object_get_role (obj);

  if ((role == ATK_ROLE_FRAME) &&
      (strcmp (atk_object_get_name (obj), "menus") == 0))
  {
    selection_obj = _find_object (obj, ATK_ROLE_MENU_BAR);
    if (selection_obj)
    {
      g_object_unref (selection_obj);
    }
  }
  else if (role == ATK_ROLE_COMBO_BOX)
  {
    selection_obj = obj;
  }
  else
    return;

  g_print ("*** Start do_selection ***\n");

  
  if (!selection_obj)
  {
    g_print ("no selection_obj\n");
    return;
  }

  i = atk_selection_get_selection_count (ATK_SELECTION (selection_obj));
  if (i != 0)
  {
    for (i = 0; i < atk_object_get_n_accessible_children (selection_obj); i++)
    {
      if (atk_selection_is_child_selected (ATK_SELECTION (selection_obj), i))
      {
        g_print ("%d child selected\n", i);
      }
      else
      {
        g_print ("%d child not selected\n", i);
      }
    } 
  } 
  /*
   * Should not be able to select all items on a menu bar
   */
  atk_selection_select_all_selection (ATK_SELECTION (selection_obj));
  i = atk_selection_get_selection_count (ATK_SELECTION (selection_obj));
  if ( i != 0)
  {
    g_print ("Unexpected selection count: %d, expected 0\n", i);
    return;
  }
  /*
   * There should not be any items selected
   */
  selected = atk_selection_ref_selection (ATK_SELECTION (selection_obj), 0);
  if ( selected != NULL)
  {
    g_print ("Unexpected selection: %d, expected 0\n", i);
  }
  atk_selection_add_selection (ATK_SELECTION (selection_obj), 1);
  g_timeout_add (2000, _finish_selection, selection_obj);
  g_print ("*** End _do_selection ***\n");
} 

static gint _finish_selection (gpointer data)
{
  AtkObject *obj = ATK_OBJECT (data);
  AtkObject *selected;
  gint      i;
  gboolean is_selected;

  g_print ("*** Start Finish selection ***\n");

  /*
   * If being run for for menus, at this point menu item foo should be 
   * selected which means that its submenu should be visible.
   */
  i = atk_selection_get_selection_count (ATK_SELECTION (obj));
  if (i != 1)
  {
    g_print ("Unexpected selection count: %d, expected 1\n", i);
    return FALSE;
  }
  selected = atk_selection_ref_selection (ATK_SELECTION (obj), 0);
  g_return_val_if_fail (selected != NULL, FALSE);
  g_print ("*** Selected Item ***\n");
  _print_type (selected);
  g_object_unref (selected);
  is_selected = atk_selection_is_child_selected (ATK_SELECTION (obj), 1);
  g_return_val_if_fail (is_selected, FALSE);
  is_selected = atk_selection_is_child_selected (ATK_SELECTION (obj), 0);
  g_return_val_if_fail (!is_selected, FALSE);
  selected = atk_selection_ref_selection (ATK_SELECTION (obj), 1);
  g_return_val_if_fail (selected == NULL, FALSE);
  atk_selection_remove_selection (ATK_SELECTION (obj), 0);
  i = atk_selection_get_selection_count (ATK_SELECTION (obj));
  g_return_val_if_fail (i == 0, FALSE);
  selected = atk_selection_ref_selection (ATK_SELECTION (obj), 0);
  g_return_val_if_fail (selected == NULL, FALSE);
  g_print ("*** End Finish selection ***\n");
  return FALSE;
}

static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_do_selection);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("testselection Module loaded\n");

  _create_event_watcher();

  return 0;
}

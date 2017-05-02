#include <stdio.h>

#include <glib.h>
#include <atk/atk.h>
#include <gtk/gtk.h>
#include "testlib.h"

#define NUM_VALID_ROLES 1

static void _print_type (AtkObject *obj);
static void _do_selection (AtkObject *obj);
static gint _finish_selection (gpointer data);
static gint _remove_page (gpointer data);

static void _print_type (AtkObject *obj)
{
  const gchar *typename = NULL;
  const gchar *name = NULL;
  const gchar *description = NULL;
  AtkRole role;

  if (GTK_IS_ACCESSIBLE (obj))
  {
    GtkWidget* widget = NULL;

    widget = GTK_ACCESSIBLE (obj)->widget;
    typename = g_type_name (G_OBJECT_TYPE (widget));
    g_print ("\tWidget type name: %s\n", typename ? typename : "NULL");
  }

  typename = g_type_name (G_OBJECT_TYPE (obj));
  g_print ("\tAccessible type name: %s\n", typename ? typename : "NULL");
  
  name = atk_object_get_name (obj);
  g_print("\tAccessible Name: %s\n", (name) ? name : "NULL");
  
  role = atk_object_get_role(obj);
  g_print ("\tAccessible Role: %d\n", role);
  
  description = atk_object_get_description (obj);
  g_print ("\tAccessible Description: %s\n", (description) ? description : "NULL");
  if (role ==  ATK_ROLE_PAGE_TAB)
  {
    AtkObject *parent, *child;
    gint x, y, width, height;

    x = y = width = height = 0;
    atk_component_get_extents (ATK_COMPONENT (obj), &x, &y, &width, &height,
                               ATK_XY_SCREEN);
    g_print ("obj: x: %d y: %d width: %d height: %d\n", x, y, width, height);
    x = y = width = height = 0;
    atk_component_get_extents (ATK_COMPONENT (obj), &x, &y, &width, &height,
                               ATK_XY_WINDOW);
    g_print ("obj: x: %d y: %d width: %d height: %d\n", x, y, width, height);
    parent = atk_object_get_parent (obj);
    x = y = width = height = 0;
    atk_component_get_extents (ATK_COMPONENT (parent), &x, &y, &width, &height,
                               ATK_XY_SCREEN);
    g_print ("parent: x: %d y: %d width: %d height: %d\n", x, y, width, height);
    x = y = width = height = 0;
    atk_component_get_extents (ATK_COMPONENT (parent), &x, &y, &width, &height,
                               ATK_XY_WINDOW);
    g_print ("parent: x: %d y: %d width: %d height: %d\n", x, y, width, height);

    child = atk_object_ref_accessible_child (obj, 0);
    x = y = width = height = 0;
    atk_component_get_extents (ATK_COMPONENT (child), &x, &y, &width, &height,
                               ATK_XY_SCREEN);
    g_print ("child: x: %d y: %d width: %d height: %d\n", x, y, width, height);
    x = y = width = height = 0;
    atk_component_get_extents (ATK_COMPONENT (child), &x, &y, &width, &height,
                               ATK_XY_WINDOW);
    g_print ("child: x: %d y: %d width: %d height: %d\n", x, y, width, height);

    g_object_unref (child);
  }
}


static void
_do_selection (AtkObject *obj)
{
  gint i;
  gint n_children;
  AtkRole role;
  AtkObject *selection_obj;
  static gboolean done_selection = FALSE;
  
  if (done_selection)
    return;

  role = atk_object_get_role (obj);

  if (role == ATK_ROLE_FRAME)
  {
    AtkRole roles[NUM_VALID_ROLES];

    roles[0] = ATK_ROLE_PAGE_TAB_LIST;

    selection_obj = find_object_by_role (obj, roles, NUM_VALID_ROLES);

    if (selection_obj)
    {
      done_selection = TRUE;
    }
    else
      return;
  }
  else
  {
    return;
  }

  g_print ("*** Start do_selection ***\n");
  
  n_children = atk_object_get_n_accessible_children (selection_obj);
  g_print ("*** Number of notebook pages: %d\n", n_children); 

  for (i = 0; i < n_children; i++)
  {
    if (atk_selection_is_child_selected (ATK_SELECTION (selection_obj), i))
    {
      g_print ("%d page selected\n", i);
    }
    else
    {
      g_print ("%d page not selected\n", i);
    }
  }
  /*
   * Should not be able to select all items in a notebook.
   */
  atk_selection_select_all_selection (ATK_SELECTION (selection_obj));
  i = atk_selection_get_selection_count (ATK_SELECTION (selection_obj));
  if ( i != 1)
  {
    g_print ("Unexpected selection count: %d, expected 1\n", i);
    g_print ("\t value of i is: %d\n", i);
    return;
  }

  for (i = 0; i < n_children; i++)
  {
    atk_selection_add_selection (ATK_SELECTION (selection_obj), i);
	
    if (atk_selection_is_child_selected (ATK_SELECTION (selection_obj), i))
    {
      g_print ("Page %d: successfully selected\n", i);
      _finish_selection (selection_obj);
    }
    else
    {
      g_print ("ERROR: child %d: selection failed\n", i);
    }
  }
  g_print ("*** End _do_selection ***\n");
  g_timeout_add (5000, _remove_page, selection_obj);
} 

static gint _remove_page (gpointer data)
{
  AtkObject *obj = ATK_OBJECT (data);
  GtkWidget *widget = NULL;

  if (GTK_IS_ACCESSIBLE (obj))
    widget = GTK_ACCESSIBLE (obj)->widget;
     
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  gtk_notebook_remove_page (GTK_NOTEBOOK (widget), 4);
  return FALSE;
}

static gint _finish_selection (gpointer data)
{
  AtkObject *obj = ATK_OBJECT (data);
  AtkObject *selected;
  AtkObject *parent_object;
  GtkWidget *parent_widget;
  gint      i, index;

  g_print ("\t*** Start Finish selection ***\n");
  
  i = atk_selection_get_selection_count (ATK_SELECTION (obj));
  if (i != 1)
  {
    g_print ("\tUnexpected selection count: %d, expected 1\n", i);
    return FALSE;
  }
  selected = atk_selection_ref_selection (ATK_SELECTION (obj), 0);
  g_return_val_if_fail (selected != NULL, FALSE);
  
  g_print ("\t*** Selected Item ***\n");
  index = atk_object_get_index_in_parent (selected);
  g_print ("\tIndex in parent is: %d\n", index);
  
  parent_object = atk_object_get_parent (selected);
  g_return_val_if_fail (ATK_IS_OBJECT (parent_object), FALSE);
  g_return_val_if_fail (parent_object == obj, FALSE);
  parent_widget = GTK_ACCESSIBLE (parent_object)->widget;
  g_return_val_if_fail (GTK_IS_NOTEBOOK (parent_widget), FALSE);
  
  _print_type (selected);
  i = atk_selection_get_selection_count (ATK_SELECTION (obj));
  g_return_val_if_fail (i == 1, FALSE);
  g_object_unref (selected);
  g_print ("\t*** End Finish selection ***\n");
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
  g_print("testnotebook Module loaded\n");

  _create_event_watcher();

  return 0;
}

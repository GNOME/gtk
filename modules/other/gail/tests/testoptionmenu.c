#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>

#include "testlib.h"

/*
 * This module is used to test the accessible implementation for GtkOptionMenu
 *
 * When the GtkOption menu in the FileSelectionDialog is tabbed to, the menu
 * is opened and the second item in the menu is selected which causes the 
 * menu to be closed and the item in the GtkOptionMenu to be updated.
 */
#define NUM_VALID_ROLES 1

static void _create_event_watcher (void);
static void _check_object (AtkObject *obj);
static gint _do_menu_item_action (gpointer data);
static gboolean doing_action = FALSE;

static void 
_check_object (AtkObject *obj)
{
  AtkRole role;
  static const char *name = NULL;
  static gboolean first_time = TRUE;

  role = atk_object_get_role (obj);
  if (role == ATK_ROLE_PUSH_BUTTON)
  /*
   * Find the specified optionmenu item
   */
    {
      AtkRole valid_roles[NUM_VALID_ROLES];
      AtkObject *atk_option_menu;
      GtkWidget *widget;

      if (name == NULL)
      {
        name = g_getenv ("TEST_ACCESSIBLE_NAME");
        if (name == NULL)
          name = "foo";
      }
      valid_roles[0] = ATK_ROLE_PUSH_BUTTON;
      atk_option_menu = find_object_by_accessible_name_and_role (obj, name,
                               valid_roles, NUM_VALID_ROLES);

      if (atk_option_menu == NULL)
        {
          g_print ("Object not found for %s\n", name);
          return;
        }
      else
        {
          g_print ("Object found for %s\n", name);
        }


      g_assert (GTK_IS_ACCESSIBLE (atk_option_menu));
      widget = GTK_ACCESSIBLE (atk_option_menu)->widget;
      g_assert (GTK_IS_OPTION_MENU (widget));

      if (first_time)
        first_time = FALSE;
      else
        return;

      /*
       * This action opens the GtkOptionMenu whose name is "foo" or whatever
       * was specified in the environment variable TEST_ACCESSIBLE_NAME
       */
      atk_action_do_action (ATK_ACTION (atk_option_menu), 0);
    }
  else if ((role == ATK_ROLE_MENU_ITEM) ||
           (role == ATK_ROLE_CHECK_MENU_ITEM) ||
           (role == ATK_ROLE_RADIO_MENU_ITEM) ||
           (role == ATK_ROLE_TEAR_OFF_MENU_ITEM))
    {
      AtkObject *parent, *child;
      AtkRole parent_role;

      /*
       * If we receive focus while waiting for the menu to be closed
       * we return immediately
       */
      if (doing_action)
        return;

      parent = atk_object_get_parent (obj);
      parent_role = atk_object_get_role (parent);
      g_assert (parent_role == ATK_ROLE_MENU);
    
      child = atk_object_ref_accessible_child (parent, 1);
      doing_action = TRUE;
      g_timeout_add (5000, _do_menu_item_action, child);
    }
  else
    {
      const char *accessible_name;

      accessible_name = atk_object_get_name (obj);
      if (accessible_name)
        {
          g_print ("Name: %s\n", accessible_name);
        } 
      else if (GTK_IS_ACCESSIBLE (obj))
        {
          GtkWidget *widget = GTK_ACCESSIBLE (obj)->widget;
          g_print ("Type: %s\n", g_type_name (G_OBJECT_TYPE (widget)));
        } 
      if (role == ATK_ROLE_TABLE)
        {
          gint n_cols, i;

          n_cols = atk_table_get_n_columns (ATK_TABLE (obj));
          g_print ("Number of Columns: %d\n", n_cols);

          for (i  = 0; i < n_cols; i++)
            {
              AtkObject *header;

              header = atk_table_get_column_header (ATK_TABLE (obj), i);
              g_print ("header: %s %s\n", 
                           g_type_name (G_OBJECT_TYPE (header)),
                           atk_object_get_name (header));
            }
        }
    }
}

static gint _do_menu_item_action (gpointer data)
{
  AtkObject *obj = ATK_OBJECT (data);

  atk_action_do_action (ATK_ACTION (obj), 0);
  doing_action = FALSE;

  g_object_unref (obj);

  return FALSE;
}

static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_check_object);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("testoptionmenu Module loaded\n");

  _create_event_watcher();

  return 0;
}

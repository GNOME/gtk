#include <string.h>
#include <gtk/gtk.h>
#include "testlib.h"

/*
 * This module is used to test the accessible implementation for menu items
 *
 * 1) When a menu item is clicked in testgtk, the action for the
 * item is performed.
 * 2) The name of the keybinding for the 'activate" action for a menu item
 * is output, if it exists.
 * 3) Execute the action for a menu item programatically
 */
#define NUM_VALID_ROLES 1

static void _create_event_watcher (void);
static void _check_object (AtkObject *obj);
static gint _do_menu_item_action (gpointer data);

static void 
_check_object (AtkObject *obj)
{
  AtkRole role;
  static const char *name = NULL;
  static gboolean first_time = TRUE;

  role = atk_object_get_role (obj);
  if (role == ATK_ROLE_FRAME)
  /*
   * Find the specified menu item
   */
  {
    AtkRole valid_roles[NUM_VALID_ROLES];
    AtkObject *atk_menu_item;
    GtkWidget *widget;

    if (name == NULL)
    {
      valid_roles[0] = ATK_ROLE_MENU_ITEM;

      name = g_getenv ("TEST_ACCESSIBLE_NAME");
      if (name == NULL)
        name = "foo";
    }
    atk_menu_item = find_object_by_accessible_name_and_role (obj, name,
                     valid_roles, NUM_VALID_ROLES);

    if (atk_menu_item == NULL)
    {
      g_print ("Object not found for %s\n", name);
      return;
    }

    g_assert (GTK_IS_ACCESSIBLE (atk_menu_item));
    widget = GTK_ACCESSIBLE (atk_menu_item)->widget;
    g_assert (GTK_IS_MENU_ITEM (widget));

    if (first_time)
      first_time = FALSE;
    else
      return;

    /*
     * This action opens the menu whose name is "foo" or whatever
     * was specified in the environment variable TEST_ACCESSIBLE_NAME
     */
    atk_action_do_action (ATK_ACTION (atk_menu_item), 0);
  }
  else if ((role == ATK_ROLE_MENU_ITEM) ||
           (role == ATK_ROLE_CHECK_MENU_ITEM) ||
           (role == ATK_ROLE_RADIO_MENU_ITEM) ||
           (role == ATK_ROLE_TEAR_OFF_MENU_ITEM))
  {
    const char *keybinding;
    const char *accessible_name;

    accessible_name = atk_object_get_name (obj);
    if (accessible_name)
      g_print ("Name: %s\n", accessible_name);
    g_print ("Action: %s\n", atk_action_get_name (ATK_ACTION (obj), 0));
    keybinding = atk_action_get_keybinding (ATK_ACTION (obj), 0);
    if (keybinding)
      g_print ("KeyBinding: %s\n", keybinding);
    /*
     * Do the action associated with the menu item once, otherwise
     * we get into a loop
     */
    if (strcmp (name, accessible_name) == 0)
    {
      if (first_time)
        first_time = FALSE;
      else
        return;
      if (g_getenv ("TEST_ACCESSIBLE_AUTO"))
        {
          g_idle_add (_do_menu_item_action, obj);
        }
    }
  }
  else
  {
    const char *accessible_name;

    accessible_name = atk_object_get_name (obj);
    if (accessible_name)
      g_print ("Name: %s\n", accessible_name);
    else if (GTK_IS_ACCESSIBLE (obj))
    {
      GtkWidget *widget = GTK_ACCESSIBLE (obj)->widget;
      g_print ("Type: %s\n", g_type_name (G_OBJECT_TYPE (widget)));
    } 
  }
}

static gint _do_menu_item_action (gpointer data)
{
  AtkObject *obj = ATK_OBJECT (data);

  atk_action_do_action (ATK_ACTION (obj), 0);

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
  g_print("testmenuitem Module loaded\n");

  _create_event_watcher();

  return 0;
}

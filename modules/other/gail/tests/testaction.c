#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "testlib.h"

/*
 * This module is used to test the implementation of AtkAction,
 * i.e. the getting of the name and the getting and setting of description
 */

static void _create_event_watcher (void);
static void _check_object (AtkObject *obj);

static void 
_check_object (AtkObject *obj)
{
  const char *accessible_name;
  const gchar * typename = NULL;

  if (GTK_IS_ACCESSIBLE (obj))
  {
    GtkWidget* widget = NULL;

    widget = GTK_ACCESSIBLE (obj)->widget;
    typename = g_type_name (G_OBJECT_TYPE (widget));
    g_print ("Widget type name: %s\n", typename ? typename : "NULL");
  }
  typename = g_type_name (G_OBJECT_TYPE (obj));
  g_print ("Accessible type name: %s\n", typename ? typename : "NULL");
  accessible_name = atk_object_get_name (obj);
  if (accessible_name)
    g_print ("Name: %s\n", accessible_name);

  if (ATK_IS_ACTION (obj))
  {
    AtkAction *action = ATK_ACTION (obj);
    gint n_actions, i;
    const gchar *action_name;
    const gchar *action_desc;
    const gchar *action_binding;
    const gchar *desc = "Test description";
 
    n_actions = atk_action_get_n_actions (action);
    g_print ("AtkAction supported number of actions: %d\n", n_actions);
    for (i = 0; i < n_actions; i++)
    {
      action_name = atk_action_get_name (action, i);
      g_print ("Name of Action %d: %s\n", i, action_name);
      action_binding = atk_action_get_keybinding (action, i);
      if (action_binding)
        g_print ("Name of Action Keybinding %d: %s\n", i, action_binding);
     
      if (!atk_action_set_description (action, i, desc))
      {
        g_print ("atk_action_set_description failed\n");
      }
      else
      {
        action_desc = atk_action_get_description (action, i);
        if (strcmp (desc, action_desc) != 0)
        {
          g_print ("Problem with setting and getting action description\n");
        }
      }
    } 
    if (atk_action_set_description (action, n_actions, desc))
    {
      g_print ("atk_action_set_description succeeded but should not have\n");
    }
  }
}

static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_check_object);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("testaction Module loaded\n");

  _create_event_watcher();

  return 0;
}

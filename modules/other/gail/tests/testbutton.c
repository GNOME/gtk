#include <gtk/gtk.h>
#include "testlib.h"

/*
 * This module is used to test the accessible implementation for buttons
 *
 * 1) It verifies that ATK_STATE_ARMED is set when a button is pressed
 * To check this click on the button whose name is specified in the
 * environment variable TEST_ACCESSIBLE_NAME or "button box" if the
 * environment variable is not set.
 *
 * 2) If the environment variable TEST_ACCESSIBLE_AUTO is set the program
 * will execute the action defined for a GailButton once.
 *
 * 3) Change an inconsistent toggle button to be consistent and vice versa.
 *
 * Note that currently this code needs to be changed manually to test
 * different actions.
 */

static void _create_event_watcher (void);
static void _check_object (AtkObject *obj);
static void button_pressed_handler (GtkButton *button);
static void _print_states (AtkObject *obj);
static void _print_button_image_info(AtkObject *obj);
static gint _do_button_action (gpointer data);
static gint _toggle_inconsistent (gpointer data);
static gint _finish_button_action (gpointer data);

#define NUM_VALID_ROLES 4

static void 
_check_object (AtkObject *obj)
{
  AtkRole role;
  static gboolean first_time = TRUE;

  role = atk_object_get_role (obj);
  if (role == ATK_ROLE_FRAME)
  /*
   * Find the specified button in the window
   */
  {
    AtkRole valid_roles[NUM_VALID_ROLES];
    const char *name;
    AtkObject *atk_button;
    GtkWidget *widget;

    valid_roles[0] = ATK_ROLE_PUSH_BUTTON;
    valid_roles[1] = ATK_ROLE_TOGGLE_BUTTON;
    valid_roles[2] = ATK_ROLE_CHECK_BOX;
    valid_roles[3] = ATK_ROLE_RADIO_BUTTON;

    name = g_getenv ("TEST_ACCESSIBLE_NAME");
    if (name == NULL)
      name = "button box";
    atk_button = find_object_by_accessible_name_and_role (obj, name,
                     valid_roles, NUM_VALID_ROLES);

    if (atk_button == NULL)
    {
      g_print ("Object not found for %s\n", name);
      return;
    }
    g_assert (GTK_IS_ACCESSIBLE (atk_button));
    widget = GTK_ACCESSIBLE (atk_button)->widget;
    g_assert (GTK_IS_BUTTON (widget));
    g_signal_connect (GTK_OBJECT (widget),
                      "pressed",
                      G_CALLBACK (button_pressed_handler),
                      NULL);
    if (GTK_IS_TOGGLE_BUTTON (widget))
    {
      _toggle_inconsistent (GTK_TOGGLE_BUTTON (widget));
    }
    if (first_time)
      first_time = FALSE;
    else
      return;

    if (g_getenv ("TEST_ACCESSIBLE_AUTO"))
      {
        g_idle_add (_do_button_action, atk_button);
      }
  }
}

static gint _toggle_inconsistent (gpointer data)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (data);

  if (gtk_toggle_button_get_inconsistent (toggle_button))
  {
    gtk_toggle_button_set_inconsistent (toggle_button, FALSE);
  }
  else
  {
    gtk_toggle_button_set_inconsistent (toggle_button, TRUE);
  }
  return FALSE;
} 

static gint _do_button_action (gpointer data)
{
  AtkObject *obj = ATK_OBJECT (data);

  atk_action_do_action (ATK_ACTION (obj), 2);

  g_timeout_add (5000, _finish_button_action, obj);
  return FALSE;
}

static gint _finish_button_action (gpointer data)
{
#if 0
  AtkObject *obj = ATK_OBJECT (data);

  atk_action_do_action (ATK_ACTION (obj), 0);
#endif

  return FALSE;
}

static void
button_pressed_handler (GtkButton *button)
{
  AtkObject *obj;

  obj = gtk_widget_get_accessible (GTK_WIDGET (button));
  _print_states (obj);
  _print_button_image_info (obj);

  if (GTK_IS_TOGGLE_BUTTON (button))
    {
      g_idle_add (_toggle_inconsistent, GTK_TOGGLE_BUTTON (button));
    }
}

static void 
_print_states (AtkObject *obj)
{
  AtkStateSet *state_set;
  gint i;

  state_set = atk_object_ref_state_set (obj);

  g_print ("*** Start states ***\n");
  for (i = 0; i < 64; i++)
  {
     AtkStateType one_state;
     const gchar *name;

     if (atk_state_set_contains_state (state_set, i))
     {
       one_state = i;

       name = atk_state_type_get_name (one_state);

       if (name)
         g_print("%s\n", name);
     }
  }
  g_object_unref (state_set);
  g_print ("*** End states ***\n");
}

static void 
_print_button_image_info(AtkObject *obj) {

  gint height, width;
  const gchar *desc;

  height = width = 0;

  if(!ATK_IS_IMAGE(obj)) 
	return;

  g_print("*** Start Button Image Info ***\n");
  desc = atk_image_get_image_description(ATK_IMAGE(obj));
  g_print ("atk_image_get_image_desc returns : %s\n", desc ? desc : "<NULL>");
  atk_image_get_image_size(ATK_IMAGE(obj), &height ,&width);
  g_print("atk_image_get_image_size returns: height %d width %d\n",height,width);
  if(atk_image_set_image_description(ATK_IMAGE(obj), "New image Description")){
	desc = atk_image_get_image_description(ATK_IMAGE(obj));
	g_print ("atk_image_get_image_desc now returns : %s\n",desc ?desc:"<NULL>");
  }
  g_print("*** End Button Image Info ***\n");


}

static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_check_object);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("testbutton Module loaded\n");

  _create_event_watcher();

  return 0;
}

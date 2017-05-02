#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <testlib.h>

static gint _test_paned (gpointer data);
static void _check_paned (AtkObject *obj);

static void _property_change_handler (AtkObject   *obj,
                                      AtkPropertyValues *values);

#define NUM_VALID_ROLES 1
static gint last_position;

static void _property_change_handler (AtkObject   *obj,
                                      AtkPropertyValues   *values)
{
  const gchar *type_name = g_type_name (G_TYPE_FROM_INSTANCE (obj));
  const gchar *name = atk_object_get_name (obj);

  g_print ("_property_change_handler: Accessible Type: %s\n",
           type_name ? type_name : "NULL");
  g_print ("_property_change_handler: Accessible name: %s\n",
           name ? name : "NULL");
  g_print ("_property_change_handler: PropertyName: %s\n",
           values->property_name ? values->property_name: "NULL");
  if (strcmp (values->property_name, "accessible-value") == 0)
  {
    GValue *value, val;
    int position;
    value = &val;

    memset (value, 0, sizeof (GValue));
    atk_value_get_current_value (ATK_VALUE (obj), value);
    g_return_if_fail (G_VALUE_HOLDS_INT (value));
    position = g_value_get_int (value); 
    g_print ("Position is  %d previous position was %d\n", 
             position, last_position);
    last_position = position;
    atk_value_get_minimum_value (ATK_VALUE (obj), value);
    g_return_if_fail (G_VALUE_HOLDS_INT (value));
    position = g_value_get_int (value); 
    g_print ("Minimum Value is  %d\n", position); 
    atk_value_get_maximum_value (ATK_VALUE (obj), value);
    g_return_if_fail (G_VALUE_HOLDS_INT (value));
    position = g_value_get_int (value); 
    g_print ("Maximum Value is  %d\n", position); 
  }
}
 
static gint _test_paned (gpointer data)
{
  AtkObject *obj = ATK_OBJECT (data);
  AtkRole role = atk_object_get_role (obj);
  GtkWidget *widget;
  static gint times = 0;

  widget = GTK_ACCESSIBLE (obj)->widget;

  if (role == ATK_ROLE_SPLIT_PANE)
  {
    GValue *value, val;
    int position;
    value = &val;

    memset (value, 0, sizeof (GValue));
    atk_value_get_current_value (ATK_VALUE (obj), value);
    g_return_val_if_fail (G_VALUE_HOLDS_INT (value), FALSE);
    position = g_value_get_int (value); 
    g_print ("Position is : %d\n", position);
    last_position = position;
    position *= 2;
    g_value_set_int (value, position);
    atk_value_set_current_value (ATK_VALUE (obj), value);
    times++;
  }
  if (times < 4)
    return TRUE;
  else
    return FALSE;
}

static void _check_paned (AtkObject *obj)
{
  static gboolean done_paned = FALSE;
  AtkRole role;

  role = atk_object_get_role (obj);

  if (role == ATK_ROLE_FRAME)
  {
    AtkRole roles[NUM_VALID_ROLES];
    AtkObject *paned_obj;

    if (done_paned)
      return;

    roles[0] = ATK_ROLE_SPLIT_PANE;

    paned_obj = find_object_by_role (obj, roles, NUM_VALID_ROLES);

    if (paned_obj)
    {
      if (!done_paned)
      {
        done_paned = TRUE;
      }
      atk_object_connect_property_change_handler (paned_obj,
                   (AtkPropertyChangeHandler*) _property_change_handler);
      g_timeout_add (2000, _test_paned, paned_obj);
    }

    return;
  }
  if (role != ATK_ROLE_COMBO_BOX)
    return;
}

static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_check_paned);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("testpaned Module loaded\n");

  _create_event_watcher();

  return 0;
}

#include <string.h>
#include <glib-object.h>
#include <atk/atk.h>

/*
 * To use this test module, run the test program testgtk and click on 
 * statusbar
 */

static void _check_statusbar (AtkObject *obj);
static AtkObject* _find_object (AtkObject* obj, AtkRole role);
static void _notify_handler (GObject *obj, GParamSpec *pspec);
static void _property_change_handler (AtkObject   *obj,
                                      AtkPropertyValues *values);

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
  if (G_VALUE_HOLDS_STRING (&values->new_value))
    g_print ("_property_change_handler: PropertyValue: %s\n",
             g_value_get_string (&values->new_value));
}

static void _check_statusbar (AtkObject *obj)
{
  AtkRole role;
  AtkObject *statusbar, *label;

  role = atk_object_get_role (obj);
  if (role != ATK_ROLE_FRAME)
    return;

  statusbar = _find_object (obj, ATK_ROLE_STATUSBAR); 
  if (!statusbar)
    return;
  g_print ("_check_statusbar\n");
  label = atk_object_ref_accessible_child (statusbar, 0);
  g_return_if_fail (label == NULL);

  /*
   * We get notified of changes to the label
   */
  g_signal_connect_closure_by_id (statusbar,
                                  g_signal_lookup ("notify", 
                                                   G_OBJECT_TYPE (statusbar)),
                                  0,
                                  g_cclosure_new (G_CALLBACK (_notify_handler),
                                                 NULL, NULL),
                                  FALSE);
  atk_object_connect_property_change_handler (statusbar,
                   (AtkPropertyChangeHandler*) _property_change_handler);

}

static void 
_notify_handler (GObject *obj, GParamSpec *pspec)
{
  AtkObject *atk_obj = ATK_OBJECT (obj);
  const gchar *name;

  g_print ("_notify_handler: property: %s\n", pspec->name);
  if (strcmp (pspec->name, "accessible-name") == 0)
  {
    name = atk_object_get_name (atk_obj);
    g_print ("_notify_handler: value: |%s|\n", name ? name : "<NULL>");
  }
}

static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_check_statusbar);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("teststatusbar Module loaded\n");

  _create_event_watcher();

  return 0;
}

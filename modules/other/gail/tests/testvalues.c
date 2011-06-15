#include <string.h>
#include <atk/atk.h>
#include <gtk/gtk.h>

static void _traverse_children (AtkObject *obj);
static void _add_handler (AtkObject *obj);
static void _check_values (AtkObject *obj);
static void _value_change_handler (AtkObject   *obj,
                                      AtkPropertyValues *values);

static guint id;

static void _value_change_handler (AtkObject   *obj,
                                   AtkPropertyValues   *values)
{
  const gchar *type_name = g_type_name (G_TYPE_FROM_INSTANCE (obj));
   GValue *value_back, val;

  value_back = &val;
    
  if (!ATK_IS_VALUE (obj)) { 
   	return;
  }

  if (strcmp (values->property_name, "accessible-value") == 0) {
	g_print ("_value_change_handler: Accessible Type: %s\n",
           type_name ? type_name : "NULL");
	if(G_VALUE_HOLDS_DOUBLE (&values->new_value))
    {
		g_print( "adjustment value changed : new value: %f\n", 
		g_value_get_double (&values->new_value));
 	}

	g_print("Now calling the AtkValue interface functions\n");

  	atk_value_get_current_value (ATK_VALUE(obj), value_back);
  	g_return_if_fail (G_VALUE_HOLDS_DOUBLE (value_back));
  	g_print ("atk_value_get_current_value returns %f\n",
			g_value_get_double (value_back)	);

  	atk_value_get_maximum_value (ATK_VALUE (obj), value_back);
  	g_return_if_fail (G_VALUE_HOLDS_DOUBLE (value_back));
  	g_print ("atk_value_get_maximum returns %f\n",
			g_value_get_double (value_back));

  	atk_value_get_minimum_value (ATK_VALUE (obj), value_back);
  	g_return_if_fail (G_VALUE_HOLDS_DOUBLE (value_back));
  	g_print ("atk_value_get_minimum returns %f\n", 
			g_value_get_double (value_back));
	
 
    }
  
}

static void _traverse_children (AtkObject *obj)
{
  gint n_children, i;

  n_children = atk_object_get_n_accessible_children (obj);
  for (i = 0; i < n_children; i++)
  {
    AtkObject *child;

    child = atk_object_ref_accessible_child (obj, i);
    _add_handler (child);
    _traverse_children (child);
    g_object_unref (G_OBJECT (child));
  }
}

static void _add_handler (AtkObject *obj)
{
  static GPtrArray *obj_array = NULL;
  gboolean found = FALSE;
  gint i;

  /*
   * We create a property handler for each object if one was not associated 
   * with it already.
   *
   * We add it to our array of objects which have property handlers; if an
   * object is destroyed it remains in the array.
   */
  if (obj_array == NULL)
    obj_array = g_ptr_array_new ();
 
  for (i = 0; i < obj_array->len; i++)
  {
    if (obj == g_ptr_array_index (obj_array, i))
    {
      found = TRUE;
      break;
    }
  }
  if (!found)
  {
    atk_object_connect_property_change_handler (obj,
                   (AtkPropertyChangeHandler*) _value_change_handler);
    g_ptr_array_add (obj_array, obj);
  }
}

static void _set_values (AtkObject *obj) {

  GValue *value_back, val;
  static gint count = 0;
  gdouble double_value;

  value_back = &val;

  if(ATK_IS_VALUE(obj)) {
	/* Spin button also inherits the text interfaces from GailEntry. 
	 * Check when spin button recieves focus.
     */

	if(ATK_IS_TEXT(obj) && ATK_IS_EDITABLE_TEXT(obj)) {
		if(count == 0) {	
			gint x;
			gchar* text;
			count++;
			x = atk_text_get_character_count (ATK_TEXT (obj));
  			text = atk_text_get_text (ATK_TEXT (obj), 0, x);
			g_print("Text : %s\n", text);
			text = "5.7";
			atk_editable_text_set_text_contents(ATK_EDITABLE_TEXT(obj),text);
			g_print("Set text to %s\n",text);
			atk_value_get_current_value(ATK_VALUE(obj), value_back);
			g_return_if_fail (G_VALUE_HOLDS_DOUBLE (value_back));
			g_print("atk_value_get_current_value returns %f\n", 
				g_value_get_double( value_back));
			} 
	} else {
		memset (value_back, 0, sizeof (GValue));
		g_value_init (value_back, G_TYPE_DOUBLE);
		g_value_set_double (value_back, 10.0);	
		if (atk_value_set_current_value (ATK_VALUE (obj), value_back))
		{
 			double_value = g_value_get_double (value_back);
  			g_print("atk_value_set_current_value returns %f\n", 
			double_value);
		}
	}
  }
}

static void _check_values (AtkObject *obj)
{
  static gint calls = 0;
  AtkRole role;

  g_print ("Start of _check_values\n");

  _set_values(obj);

  _add_handler (obj);

  if (++calls < 2)
  { 
    /*
     * Just do this on this on the first 2 objects visited
     */
    atk_object_set_name (obj, "test123");
    atk_object_set_description (obj, "test123");
  }

  role = atk_object_get_role (obj);

  if (role == ATK_ROLE_FRAME || role == ATK_ROLE_DIALOG)
  {
    /*
     * Add handlers to all children.
     */
    _traverse_children (obj);
  }
  g_print ("End of _check_values\n");
}

static void
_create_event_watcher (void)
{
  id = atk_add_focus_tracker (_check_values);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("testvalues Module loaded\n");

  _create_event_watcher();

  return 0;
}

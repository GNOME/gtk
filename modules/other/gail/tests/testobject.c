#include <gtk/gtk.h>
#include "testlib.h"

static void _print_accessible (AtkObject *obj);
static void _print_type (AtkObject *obj);
static void _print_states (AtkObject *obj);
static void _check_children (AtkObject *obj);
static void _check_relation (AtkObject *obj);
static void _create_event_watcher (void);
static void _focus_handler (AtkObject *obj, gboolean focus_in);
static gboolean _children_changed (GSignalInvocationHint *ihint,
                                   guint                  n_param_values,
                                   const GValue          *param_values,
                                   gpointer               data);

static guint id;

static void _print_states (AtkObject *obj)
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

static void _print_type (AtkObject *obj)
{
  const gchar * typename = NULL;
  const gchar * name = NULL;
  AtkRole role;
  static gboolean in_print_type = FALSE;
   
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
  role = atk_object_get_role (obj);
  g_print ("Accessible Role: %s\n", atk_role_get_name (role));

  if (ATK_IS_COMPONENT (obj))
    {
      gint x, y, width, height;
      AtkStateSet *states;

      _print_states (obj);
      states = atk_object_ref_state_set (obj);
      if (atk_state_set_contains_state (states, ATK_STATE_VISIBLE))
        {
          AtkObject *parent;

          atk_component_get_extents (ATK_COMPONENT (obj), 
                                     &x, &y, &width, &height, 
                                     ATK_XY_SCREEN);
          g_print ("ATK_XY_SCREEN: x: %d y: %d width: %d height: %d\n",
                   x, y, width, height);

          atk_component_get_extents (ATK_COMPONENT (obj), 
                                     &x, &y, &width, &height, 
                                     ATK_XY_WINDOW);
          g_print ("ATK_XY_WINDOW: x: %d y: %d width: %d height: %d\n", 
                   x, y, width, height);
          if (atk_state_set_contains_state (states, ATK_STATE_SHOWING) &&
              ATK_IS_TEXT (obj))
            {
              gint offset;

              atk_text_get_character_extents (ATK_TEXT (obj), 1, 
                                              &x, &y, &width, &height, 
                                              ATK_XY_WINDOW);
              g_print ("Character extents : %d %d %d %d\n", 
                       x, y, width, height);
              if (width != 0 && height != 0)
                {
                  offset = atk_text_get_offset_at_point (ATK_TEXT (obj), 
                                                         x, y, 
                                                         ATK_XY_WINDOW);
                  if (offset != 1)
                    {
                      g_print ("Wrong offset returned (%d) %d\n", 1, offset);
                    }
                }
            }
          if (in_print_type)
            return;

          parent = atk_object_get_parent (obj);
          if (!ATK_IS_COMPONENT (parent))
            {
              /* Assume toplevel */
              g_object_unref (G_OBJECT (states));
              return;
            }
#if 0
          obj1 = atk_component_ref_accessible_at_point (ATK_COMPONENT (parent),
                                                        x, y, ATK_XY_WINDOW);
          if (obj != obj1)
            {
              g_print ("Inconsistency between object and ref_accessible_at_point\n");
              in_print_type = TRUE;
              _print_type (obj1);
              in_print_type = FALSE;
            }
#endif
        }
      g_object_unref (G_OBJECT (states));
    }
}

static void _print_accessible (AtkObject *obj)
{
  GtkWidget* widget = NULL;
  AtkObject* parent_atk;
  AtkObject* ref_obj;
  AtkRole    role;
  static gboolean first_time = TRUE;

  if (first_time)
    {
      first_time = FALSE;
      atk_add_global_event_listener (_children_changed, 
                                     "Atk:AtkObject:children_changed");
    }

  /*
   * Check that the object returned by the atk_implementor_ref_accessible()
   * for a widget is the same as the accessible object
   */
  if (GTK_IS_ACCESSIBLE (obj))
    {
      widget = GTK_ACCESSIBLE (obj)->widget;
      ref_obj = atk_implementor_ref_accessible (ATK_IMPLEMENTOR (widget));
      g_assert (ref_obj == obj);
      g_object_unref (G_OBJECT (ref_obj));
    }
  /*
   * Add a focus handler so we see focus out events as well
   */
  if (ATK_IS_COMPONENT (obj))
    atk_component_add_focus_handler (ATK_COMPONENT (obj), _focus_handler);
  g_print ("Object:\n");
  _print_type (obj);
  _print_states (obj);

  /*
   * Get the parent object
   */
  parent_atk = atk_object_get_parent (obj);
  if (parent_atk)
    {
      g_print ("Parent Object:\n");
      _print_type (parent_atk);
      parent_atk = atk_object_get_parent (parent_atk);
      if (parent_atk)
        {
          g_print ("Grandparent Object:\n");
          _print_type (parent_atk);
        }
    } 
  else 
    {
      g_print ("No parent\n");
    }

  role = atk_object_get_role (obj);

  if ((role == ATK_ROLE_FRAME) || (role == ATK_ROLE_DIALOG))
    {
      _check_children (obj);
    }
}

static void _check_relation (AtkObject *obj)
{
  AtkRelationSet* relation_set = atk_object_ref_relation_set (obj);
  gint n_relations, i;

  g_return_if_fail (relation_set);

  n_relations = atk_relation_set_get_n_relations (relation_set);
  for (i = 0; i < n_relations; i++)
    {
      AtkRelation* relation = atk_relation_set_get_relation (relation_set, i);

      g_print ("Index: %d Relation type: %d Number: %d\n", i,
                atk_relation_get_relation_type (relation),
                atk_relation_get_target (relation)->len);
    }
  g_object_unref (relation_set);
}

static void _check_children (AtkObject *obj)
{
  gint n_children, i;
  AtkLayer layer;
  AtkRole role;

  g_print ("Start Check Children\n");
  n_children = atk_object_get_n_accessible_children (obj);
  g_print ("Number of children: %d\n", n_children);

  role = atk_object_get_role (obj);

  if (ATK_IS_COMPONENT (obj))
    {
      atk_component_add_focus_handler (ATK_COMPONENT (obj), _focus_handler);
      layer = atk_component_get_layer (ATK_COMPONENT (obj));
      if (role == ATK_ROLE_MENU)
	      g_assert (layer == ATK_LAYER_POPUP);
      else
	      g_print ("Layer: %d\n", layer);
    }

  for (i = 0; i < n_children; i++)
    {
      AtkObject *child;
      AtkObject *parent;
      int j;

      child = atk_object_ref_accessible_child (obj, i);
      parent = atk_object_get_parent (child);
      j = atk_object_get_index_in_parent (child);
      _print_type (child);
      _check_relation (child);
      _check_children (child);
      if (obj != parent)
        {
          g_print ("*** Inconsistency between atk_object_get_parent() and "
                   "atk_object_ref_accessible_child() ***\n");
          _print_type (child);
          _print_type (obj);
          if (parent)
            _print_type (parent);
        }
      g_object_unref (G_OBJECT (child));
                 
      if (j != i)
        g_print ("*** Inconsistency between parent and children %d %d ***\n",
                 i, j);
    }
  g_print ("End Check Children\n");
}

static gboolean
_children_changed (GSignalInvocationHint *ihint,
                   guint                  n_param_values,
                   const GValue          *param_values,
                   gpointer               data)
{
  GObject *object;
  guint index;
  gpointer target;
  const gchar *target_name = "NotAtkObject";

  object = g_value_get_object (param_values + 0);
  index = g_value_get_uint (param_values + 1);
  target = g_value_get_pointer (param_values + 2);
  if (G_IS_OBJECT (target))
    {
      if (ATK_IS_OBJECT (target))
        {
          target_name = atk_object_get_name (target);
        }
      if (!target_name) 
        target_name = g_type_name (G_OBJECT_TYPE (G_OBJECT (target)));
    }
  else
    {
      if (!target)
        {
          AtkObject *child;

          child = atk_object_ref_accessible_child (ATK_OBJECT (object), index);
          if (child)
            {
              target_name = g_type_name (G_OBJECT_TYPE (G_OBJECT (child)));
              g_object_unref (child);
            }
        }
    }
  g_print ("_children_watched: %s %s %s index: %d\n", 
           g_type_name (G_OBJECT_TYPE (object)),
           g_quark_to_string (ihint->detail),
           target_name, index);
  return TRUE;
}

static void
_create_event_watcher (void)
{
  /*
   * _print_accessible() will be called for an accessible object when its
   * widget receives focus.
   */
  id = atk_add_focus_tracker (_print_accessible);
}

static void 
_focus_handler (AtkObject *obj, gboolean focus_in)
{
  g_print ("In _focus_handler focus_in: %s\n", focus_in ? "true" : "false"); 
  _print_type (obj);
}

int
gtk_module_init(gint argc, char* argv[])
{
  g_print("testobject Module loaded\n");

  _create_event_watcher();

  return 0;
}

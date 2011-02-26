/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtkstyleproperties.h"

#include <stdlib.h>
#include <gobject/gvaluecollector.h>
#include <cairo-gobject.h>

#include "gtktypebuiltins.h"
#include "gtkstyleprovider.h"
#include "gtksymboliccolor.h"
#include "gtkprivate.h"
#include "gtkthemingengine.h"
#include "gtkanimationdescription.h"
#include "gtkborder.h"
#include "gtkgradient.h"
#include "gtk9slice.h"
#include "gtkintl.h"

/**
 * SECTION:gtkstyleproperties
 * @Short_description: Store for style property information
 * @Title: GtkStyleProperties
 *
 * GtkStyleProperties provides the storage for style information
 * that is used by #GtkStyleContext and other #GtkStyleProvider
 * implementations.
 *
 * Before style properties can be stored in GtkStyleProperties, they
 * must be registered with gtk_style_properties_register_property().
 *
 * Unless you are writing a #GtkStyleProvider implementation, you
 * are unlikely to use this API directly, as gtk_style_context_get()
 * and its variants are the preferred way to access styling information
 * from widget implementations and theming engine implementations
 * should use the APIs provided by #GtkThemingEngine instead.
 */

typedef struct GtkStylePropertiesPrivate GtkStylePropertiesPrivate;
typedef struct PropertyData PropertyData;
typedef struct PropertyNode PropertyNode;
typedef struct ValueData ValueData;

struct PropertyNode
{
  GQuark property_quark;
  GParamSpec *pspec;
  GtkStylePropertyParser parse_func;
};

struct ValueData
{
  GtkStateFlags state;
  GValue value;
};

struct PropertyData
{
  GArray *values;
};

struct GtkStylePropertiesPrivate
{
  GHashTable *color_map;
  GHashTable *properties;
};

static GArray *properties = NULL;

static void gtk_style_properties_provider_init (GtkStyleProviderIface *iface);
static void gtk_style_properties_finalize      (GObject      *object);


G_DEFINE_TYPE_EXTENDED (GtkStyleProperties, gtk_style_properties, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_style_properties_provider_init));

static void
gtk_style_properties_class_init (GtkStylePropertiesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_properties_finalize;

  /* Initialize default property set */
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("color",
                                                              "Foreground color",
                                                              "Foreground color",
                                                              GDK_TYPE_RGBA, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("background-color",
                                                              "Background color",
                                                              "Background color",
                                                              GDK_TYPE_RGBA, 0));

  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("font",
                                                              "Font Description",
                                                              "Font Description",
                                                              PANGO_TYPE_FONT_DESCRIPTION, 0));

  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("margin",
                                                              "Margin",
                                                              "Margin",
                                                              GTK_TYPE_BORDER, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("padding",
                                                              "Padding",
                                                              "Padding",
                                                              GTK_TYPE_BORDER, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-width",
                                                              "Border width",
                                                              "Border width, in pixels",
                                                              GTK_TYPE_BORDER, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("border-radius",
                                                            "Border radius",
                                                            "Border radius, in pixels",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_enum ("border-style",
                                                             "Border style",
                                                             "Border style",
                                                             GTK_TYPE_BORDER_STYLE,
                                                             GTK_BORDER_STYLE_NONE, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-color",
                                                              "Border color",
                                                              "Border color",
                                                              GDK_TYPE_RGBA, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("background-image",
                                                              "Background Image",
                                                              "Background Image",
                                                              CAIRO_GOBJECT_TYPE_PATTERN, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-image",
                                                              "Border Image",
                                                              "Border Image",
                                                              GTK_TYPE_9SLICE, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_object ("engine",
                                                               "Theming Engine",
                                                               "Theming Engine",
                                                               GTK_TYPE_THEMING_ENGINE, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("transition",
                                                              "Transition animation description",
                                                              "Transition animation description",
                                                              GTK_TYPE_ANIMATION_DESCRIPTION, 0));

  /* Private property holding the binding sets */
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("gtk-key-bindings",
                                                              "Key bindings",
                                                              "Key bindings",
                                                              G_TYPE_PTR_ARRAY, 0));

  g_type_class_add_private (object_class, sizeof (GtkStylePropertiesPrivate));
}

static PropertyData *
property_data_new (void)
{
  PropertyData *data;

  data = g_slice_new0 (PropertyData);
  data->values = g_array_new (FALSE, FALSE, sizeof (ValueData));

  return data;
}

static void
property_data_remove_values (PropertyData *data)
{
  guint i;

  for (i = 0; i < data->values->len; i++)
    {
      ValueData *value_data;

      value_data = &g_array_index (data->values, ValueData, i);

      if (G_IS_VALUE (&value_data->value))
        g_value_unset (&value_data->value);
    }

  if (data->values->len > 0)
    g_array_remove_range (data->values, 0, data->values->len);
}

static void
property_data_free (PropertyData *data)
{
  property_data_remove_values (data);
  g_array_free (data->values, TRUE);
  g_slice_free (PropertyData, data);
}

static gboolean
property_data_find_position (PropertyData  *data,
                             GtkStateFlags  state,
                             guint         *pos)
{
  gint min, max, mid;
  gboolean found = FALSE;
  guint position;

  if (pos)
    *pos = 0;

  if (data->values->len == 0)
    return FALSE;

  /* Find position for the given state, or the position where
   * it would be if not found, the array is ordered by the
   * state flags.
   */
  min = 0;
  max = data->values->len - 1;

  do
    {
      ValueData *value_data;

      mid = (min + max) / 2;
      value_data = &g_array_index (data->values, ValueData, mid);

      if (value_data->state == state)
        {
          found = TRUE;
          position = mid;
        }
      else if (value_data->state < state)
          position = min = mid + 1;
      else
        {
          max = mid - 1;
          position = mid;
        }
    }
  while (!found && min <= max);

  if (pos)
    *pos = position;

  return found;
}

static GValue *
property_data_get_value (PropertyData  *data,
                         GtkStateFlags  state)
{
  ValueData *val_data;
  guint pos;

  if (!property_data_find_position (data, state, &pos))
    {
      ValueData new = { 0 };

      new.state = state;
      g_array_insert_val (data->values, pos, new);
    }

  val_data = &g_array_index (data->values, ValueData, pos);

  return &val_data->value;
}

static GValue *
property_data_match_state (PropertyData  *data,
                           GtkStateFlags  state)
{
  guint pos;
  gint i;

  if (property_data_find_position (data, state, &pos))
    {
      ValueData *val_data;

      /* Exact match */
      val_data = &g_array_index (data->values, ValueData, pos);
      return &val_data->value;
    }

  if (pos >= data->values->len)
    pos = data->values->len - 1;

  /* No exact match, go downwards the list to find
   * the closest match to the given state flags, as
   * a side effect, there is an implicit precedence
   * of higher flags over the smaller ones.
   */
  for (i = pos; i >= 0; i--)
    {
      ValueData *val_data;

      val_data = &g_array_index (data->values, ValueData, i);

       /* Check whether any of the requested
        * flags are set, and no other flags are.
        *
        * Also, no flags acts as a wildcard, such
        * value should be always in the first position
        * in the array (if present) anyways.
        */
      if (val_data->state == 0 ||
          ((val_data->state & state) != 0 &&
           (val_data->state & ~state) == 0))
        return &val_data->value;
    }

  return NULL;
}

static void
gtk_style_properties_init (GtkStyleProperties *props)
{
  GtkStylePropertiesPrivate *priv;

  priv = props->priv = G_TYPE_INSTANCE_GET_PRIVATE (props,
                                                    GTK_TYPE_STYLE_PROPERTIES,
                                                    GtkStylePropertiesPrivate);

  priv->properties = g_hash_table_new_full (NULL, NULL, NULL,
                                            (GDestroyNotify) property_data_free);
}

static void
gtk_style_properties_finalize (GObject *object)
{
  GtkStylePropertiesPrivate *priv;
  GtkStyleProperties *props;

  props = GTK_STYLE_PROPERTIES (object);
  priv = props->priv;
  g_hash_table_destroy (priv->properties);

  if (priv->color_map)
    g_hash_table_destroy (priv->color_map);

  G_OBJECT_CLASS (gtk_style_properties_parent_class)->finalize (object);
}

GtkStyleProperties *
gtk_style_properties_get_style (GtkStyleProvider *provider,
                                GtkWidgetPath    *path)
{
  /* Return style set itself */
  return g_object_ref (provider);
}

static void
gtk_style_properties_provider_init (GtkStyleProviderIface *iface)
{
  iface->get_style = gtk_style_properties_get_style;
}

static int
compare_property (gconstpointer p1,
                  gconstpointer p2)
{
  PropertyNode *key = (PropertyNode *) p1;
  PropertyNode *node = (PropertyNode *) p2;

  if (key->property_quark > node->property_quark)
    return 1;
  else if (key->property_quark < node->property_quark)
    return -1;

  return 0;
}

static PropertyNode *
property_node_lookup (GQuark quark)
{
  PropertyNode key = { 0 };

  if (!quark)
    return NULL;

  if (!properties)
    return NULL;

  key.property_quark = quark;

  return bsearch (&key, properties->data, properties->len,
                  sizeof (PropertyNode), compare_property);
}

/* Property registration functions */

/**
 * gtk_style_properties_register_property: (skip)
 * @parse_func: parsing function to use, or %NULL
 * @pspec: the #GParamSpec for the new property
 *
 * Registers a property so it can be used in the CSS file format.
 * This function is the low-level equivalent of
 * gtk_theming_engine_register_property(), if you are implementing
 * a theming engine, you want to use that function instead.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_register_property (GtkStylePropertyParser  parse_func,
                                        GParamSpec             *pspec)
{
  PropertyNode *node, new = { 0 };
  GQuark quark;
  gint i;

  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  if (G_UNLIKELY (!properties))
    properties = g_array_new (FALSE, TRUE, sizeof (PropertyNode));

  quark = g_quark_from_string (pspec->name);

  if ((node = property_node_lookup (quark)) != NULL)
    {
      g_warning ("Property \"%s\" was already registered with type %s",
                 pspec->name, g_type_name (node->pspec->value_type));
      return;
    }

  new.property_quark = quark;
  new.pspec = pspec;

  if (parse_func)
    new.parse_func = parse_func;

  for (i = 0; i < properties->len; i++)
    {
      node = &g_array_index (properties, PropertyNode, i);

      if (node->property_quark > quark)
        break;
    }

  g_array_insert_val (properties, i, new);
}

/**
 * gtk_style_properties_lookup_property: (skip)
 * @property_name: property name to look up
 * @parse_func: (out): return location for the parse function
 * @pspec: (out) (transfer none): return location for the #GParamSpec
 *
 * Returns %TRUE if a property has been registered, if @pspec or
 * @parse_func are not %NULL, the #GParamSpec and parsing function
 * will be respectively returned.
 *
 * Returns: %TRUE if the property is registered, %FALSE otherwise
 *
 * Since: 3.0
 **/
gboolean
gtk_style_properties_lookup_property (const gchar             *property_name,
                                      GtkStylePropertyParser  *parse_func,
                                      GParamSpec             **pspec)
{
  PropertyNode *node;
  GtkStylePropertiesClass *klass;
  gboolean found = FALSE;
  GQuark quark;
  gint i;

  g_return_val_if_fail (property_name != NULL, FALSE);

  klass = g_type_class_ref (GTK_TYPE_STYLE_PROPERTIES);
  quark = g_quark_try_string (property_name);

  if (quark == 0)
    {
      g_type_class_unref (klass);
      return FALSE;
    }

  for (i = 0; i < properties->len; i++)
    {
      node = &g_array_index (properties, PropertyNode, i);

      if (node->property_quark == quark)
        {
          if (pspec)
            *pspec = node->pspec;

          if (parse_func)
            *parse_func = node->parse_func;

          found = TRUE;
          break;
        }
      else if (node->property_quark > quark)
        break;
    }

  g_type_class_unref (klass);

  return found;
}

/* GtkStyleProperties methods */

/**
 * gtk_style_properties_new:
 *
 * Returns a newly created #GtkStyleProperties
 *
 * Returns: a new #GtkStyleProperties
 **/
GtkStyleProperties *
gtk_style_properties_new (void)
{
  return g_object_new (GTK_TYPE_STYLE_PROPERTIES, NULL);
}

/**
 * gtk_style_properties_map_color:
 * @props: a #GtkStyleProperties
 * @name: color name
 * @color: #GtkSymbolicColor to map @name to
 *
 * Maps @color so it can be referenced by @name. See
 * gtk_style_properties_lookup_color()
 *
 * Since: 3.0
 **/
void
gtk_style_properties_map_color (GtkStyleProperties *props,
                                const gchar        *name,
                                GtkSymbolicColor   *color)
{
  GtkStylePropertiesPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (name != NULL);
  g_return_if_fail (color != NULL);

  priv = props->priv;

  if (G_UNLIKELY (!priv->color_map))
    priv->color_map = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             (GDestroyNotify) g_free,
                                             (GDestroyNotify) gtk_symbolic_color_unref);

  g_hash_table_replace (priv->color_map,
                        g_strdup (name),
                        gtk_symbolic_color_ref (color));
}

/**
 * gtk_style_properties_lookup_color:
 * @props: a #GtkStyleProperties
 * @name: color name to lookup
 *
 * Returns the symbolic color that is mapped
 * to @name.
 *
 * Returns: (transfer none): The mapped color
 *
 * Since: 3.0
 **/
GtkSymbolicColor *
gtk_style_properties_lookup_color (GtkStyleProperties *props,
                                   const gchar        *name)
{
  GtkStylePropertiesPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_PROPERTIES (props), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  priv = props->priv;

  if (!priv->color_map)
    return NULL;

  return g_hash_table_lookup (priv->color_map, name);
}

/**
 * gtk_style_properties_set_property:
 * @props: a #GtkStyleProperties
 * @property: styling property to set
 * @state: state to set the value for
 * @value: new value for the property
 *
 * Sets a styling property in @props.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_set_property (GtkStyleProperties *props,
                                   const gchar        *property,
                                   GtkStateFlags       state,
                                   const GValue       *value)
{
  GtkStylePropertiesPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;
  GType value_type;
  GValue *val;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  value_type = G_VALUE_TYPE (value);
  node = property_node_lookup (g_quark_try_string (property));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }

  if (node->pspec->value_type == GDK_TYPE_RGBA ||
      node->pspec->value_type == GDK_TYPE_COLOR)
    {
      /* Allow GtkSymbolicColor as well */
      g_return_if_fail (value_type == GDK_TYPE_RGBA ||
                        value_type == GDK_TYPE_COLOR ||
                        value_type == GTK_TYPE_SYMBOLIC_COLOR);
    }
  else if (node->pspec->value_type == CAIRO_GOBJECT_TYPE_PATTERN)
    {
      /* Allow GtkGradient as a substitute */
      g_return_if_fail (value_type == CAIRO_GOBJECT_TYPE_PATTERN ||
                        value_type == GTK_TYPE_GRADIENT);
    }
  else
    g_return_if_fail (node->pspec->value_type == value_type);

  priv = props->priv;
  prop = g_hash_table_lookup (priv->properties,
                              GINT_TO_POINTER (node->property_quark));

  if (!prop)
    {
      prop = property_data_new ();
      g_hash_table_insert (priv->properties,
                           GINT_TO_POINTER (node->property_quark),
                           prop);
    }

  val = property_data_get_value (prop, state);

  if (G_VALUE_TYPE (val) == value_type)
    g_value_reset (val);
  else
    {
      if (G_IS_VALUE (val))
        g_value_unset (val);

      g_value_init (val, value_type);
    }

  g_value_copy (value, val);
}

/**
 * gtk_style_properties_set_valist:
 * @props: a #GtkStyleProperties
 * @state: state to set the values for
 * @args: va_list of property name/value pairs, followed by %NULL
 *
 * Sets several style properties on @props.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_set_valist (GtkStyleProperties *props,
                                 GtkStateFlags       state,
                                 va_list             args)
{
  GtkStylePropertiesPrivate *priv;
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  priv = props->priv;
  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      PropertyNode *node;
      PropertyData *prop;
      gchar *error = NULL;
      GValue *val;

      node = property_node_lookup (g_quark_try_string (property_name));

      if (!node)
        {
          g_warning ("Style property \"%s\" is not registered", property_name);
          break;
        }

      prop = g_hash_table_lookup (priv->properties,
                                  GINT_TO_POINTER (node->property_quark));

      if (!prop)
        {
          prop = property_data_new ();
          g_hash_table_insert (priv->properties,
                               GINT_TO_POINTER (node->property_quark),
                               prop);
        }

      val = property_data_get_value (prop, state);

      if (G_IS_VALUE (val))
        g_value_unset (val);

      G_VALUE_COLLECT_INIT (val, node->pspec->value_type,
                            args, 0, &error);
      if (error)
        {
          g_warning ("Could not set style property \"%s\": %s", property_name, error);
          g_value_unset (val);
          g_free (error);
          break;
        }

      property_name = va_arg (args, const gchar *);
    }
}

/**
 * gtk_style_properties_set:
 * @props: a #GtkStyleProperties
 * @state: state to set the values for
 * @...: property name/value pairs, followed by %NULL
 *
 * Sets several style properties on @props.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_set (GtkStyleProperties *props,
                          GtkStateFlags       state,
                          ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  va_start (args, state);
  gtk_style_properties_set_valist (props, state, args);
  va_end (args);
}

static gboolean
resolve_color (GtkStyleProperties *props,
	       GValue             *value)
{
  GdkRGBA color;

  /* Resolve symbolic color to GdkRGBA */
  if (!gtk_symbolic_color_resolve (g_value_get_boxed (value), props, &color))
    return FALSE;

  /* Store it back, this is where GdkRGBA caching happens */
  g_value_unset (value);
  g_value_init (value, GDK_TYPE_RGBA);
  g_value_set_boxed (value, &color);

  return TRUE;
}

static gboolean
resolve_color_rgb (GtkStyleProperties *props,
                   GValue             *value)
{
  GdkColor color = { 0 };
  GdkRGBA rgba;

  if (!gtk_symbolic_color_resolve (g_value_get_boxed (value), props, &rgba))
    return FALSE;

  color.red = rgba.red * 65535. + 0.5;
  color.green = rgba.green * 65535. + 0.5;
  color.blue = rgba.blue * 65535. + 0.5;

  g_value_unset (value);
  g_value_init (value, GDK_TYPE_COLOR);
  g_value_set_boxed (value, &color);

  return TRUE;
}

static gboolean
resolve_gradient (GtkStyleProperties *props,
                  GValue             *value)
{
  cairo_pattern_t *gradient;

  if (!gtk_gradient_resolve (g_value_get_boxed (value), props, &gradient))
    return FALSE;

  /* Store it back, this is where cairo_pattern_t caching happens */
  g_value_unset (value);
  g_value_init (value, CAIRO_GOBJECT_TYPE_PATTERN);
  g_value_take_boxed (value, gradient);

  return TRUE;
}

static gboolean
style_properties_resolve_type (GtkStyleProperties *props,
                               PropertyNode       *node,
                               GValue             *val)
{
  if (val && G_VALUE_TYPE (val) == GTK_TYPE_SYMBOLIC_COLOR)
    {
      if (node->pspec->value_type == GDK_TYPE_RGBA)
        {
          if (!resolve_color (props, val))
            return FALSE;
        }
      else if (node->pspec->value_type == GDK_TYPE_COLOR)
        {
          if (!resolve_color_rgb (props, val))
            return FALSE;
        }
      else
        return FALSE;
    }
  else if (val && G_VALUE_TYPE (val) == GTK_TYPE_GRADIENT)
    {
      g_return_val_if_fail (node->pspec->value_type == CAIRO_GOBJECT_TYPE_PATTERN, FALSE);

      if (!resolve_gradient (props, val))
        return FALSE;
    }

  return TRUE;
}

static void
lookup_default_value (PropertyNode *node,
                      GValue       *value)
{
  if (node->pspec->value_type == GTK_TYPE_THEMING_ENGINE)
    g_value_set_object (value, gtk_theming_engine_load (NULL));
  else if (node->pspec->value_type == PANGO_TYPE_FONT_DESCRIPTION)
    g_value_take_boxed (value, pango_font_description_from_string ("Sans 10"));
  else if (node->pspec->value_type == GDK_TYPE_RGBA)
    {
      GdkRGBA color;
      gdk_rgba_parse (&color, "pink");
      g_value_set_boxed (value, &color);
    }
  else if (node->pspec->value_type == GTK_TYPE_BORDER)
    {
      g_value_take_boxed (value, gtk_border_new ());
    }
  else
    g_param_value_set_default (node->pspec, value);
}

const GValue *
_gtk_style_properties_peek_property (GtkStyleProperties *props,
                                     const gchar        *prop_name,
                                     GtkStateFlags       state)
{
  GtkStylePropertiesPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;
  GValue *val;

  g_return_val_if_fail (GTK_IS_STYLE_PROPERTIES (props), NULL);
  g_return_val_if_fail (prop_name != NULL, NULL);

  node = property_node_lookup (g_quark_try_string (prop_name));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", prop_name);
      return NULL;
    }

  priv = props->priv;
  prop = g_hash_table_lookup (priv->properties,
                              GINT_TO_POINTER (node->property_quark));

  if (!prop)
    return NULL;

  val = property_data_match_state (prop, state);

  if (val &&
      !style_properties_resolve_type (props, node, val))
    return NULL;

  return val;
}

/**
 * gtk_style_properties_get_property:
 * @props: a #GtkStyleProperties
 * @property: style property name
 * @state: state to retrieve the property value for
 * @value: (out) (transfer full):  return location for the style property value.
 *
 * Gets a style property from @props for the given state. When done with @value,
 * g_value_unset() needs to be called to free any allocated memory.
 *
 * Returns: %TRUE if the property exists in @props, %FALSE otherwise
 *
 * Since: 3.0
 **/
gboolean
gtk_style_properties_get_property (GtkStyleProperties *props,
                                   const gchar        *property,
                                   GtkStateFlags       state,
                                   GValue             *value)
{
  GtkStylePropertiesPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;
  GValue *val;

  g_return_val_if_fail (GTK_IS_STYLE_PROPERTIES (props), FALSE);
  g_return_val_if_fail (property != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  node = property_node_lookup (g_quark_try_string (property));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return FALSE;
    }

  priv = props->priv;
  prop = g_hash_table_lookup (priv->properties,
                              GINT_TO_POINTER (node->property_quark));

  if (!prop)
    return FALSE;

  g_value_init (value, node->pspec->value_type);
  val = property_data_match_state (prop, state);

  if (val &&
      !style_properties_resolve_type (props, node, val))
    return FALSE;

  if (val)
    {
      g_param_value_validate (node->pspec, val);
      g_value_copy (val, value);
    }
  else
    lookup_default_value (node, value);

  return TRUE;
}

/**
 * gtk_style_properties_get_valist:
 * @props: a #GtkStyleProperties
 * @state: state to retrieve the property values for
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several style property values from @props for a given state.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_get_valist (GtkStyleProperties *props,
                                 GtkStateFlags       state,
                                 va_list             args)
{
  GtkStylePropertiesPrivate *priv;
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  priv = props->priv;
  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      PropertyNode *node;
      PropertyData *prop;
      gchar *error = NULL;
      GValue *val = NULL;

      node = property_node_lookup (g_quark_try_string (property_name));

      if (!node)
        {
          g_warning ("Style property \"%s\" is not registered", property_name);
          break;
        }

      prop = g_hash_table_lookup (priv->properties,
                                  GINT_TO_POINTER (node->property_quark));

      if (prop)
        val = property_data_match_state (prop, state);

      if (val &&
          !style_properties_resolve_type (props, node, val))
        val = NULL;

      if (val)
        {
          g_param_value_validate (node->pspec, val);
          G_VALUE_LCOPY (val, args, 0, &error);
        }
      else
        {
          GValue default_value = { 0 };

          g_value_init (&default_value, node->pspec->value_type);
          lookup_default_value (node, &default_value);
          G_VALUE_LCOPY (&default_value, args, 0, &error);
          g_value_unset (&default_value);
        }

      if (error)
        {
          g_warning ("Could not get style property \"%s\": %s", property_name, error);
          g_free (error);
          break;
        }

      property_name = va_arg (args, const gchar *);
    }
}

/**
 * gtk_style_properties_get:
 * @props: a #GtkStyleProperties
 * @state: state to retrieve the property values for
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several style property values from @props for a
 * given state.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_get (GtkStyleProperties *props,
                          GtkStateFlags       state,
                          ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  va_start (args, state);
  gtk_style_properties_get_valist (props, state, args);
  va_end (args);
}

/**
 * gtk_style_properties_unset_property:
 * @props: a #GtkStyleProperties
 * @property: property to unset
 * @state: state to unset
 *
 * Unsets a style property in @props.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_unset_property (GtkStyleProperties *props,
                                     const gchar        *property,
                                     GtkStateFlags       state)
{
  GtkStylePropertiesPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;
  guint pos;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (property != NULL);

  node = property_node_lookup (g_quark_try_string (property));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }

  priv = props->priv;
  prop = g_hash_table_lookup (priv->properties,
                              GINT_TO_POINTER (node->property_quark));

  if (!prop)
    return;

  if (property_data_find_position (prop, state, &pos))
    {
      ValueData *data;

      data = &g_array_index (prop->values, ValueData, pos);

      if (G_IS_VALUE (&data->value))
        g_value_unset (&data->value);

      g_array_remove_index (prop->values, pos);
    }
}

/**
 * gtk_style_properties_clear:
 * @props: a #GtkStyleProperties
 *
 * Clears all style information from @props.
 **/
void
gtk_style_properties_clear (GtkStyleProperties *props)
{
  GtkStylePropertiesPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  priv = props->priv;
  g_hash_table_remove_all (priv->properties);
}

/**
 * gtk_style_properties_merge:
 * @props: a #GtkStyleProperties
 * @props_to_merge: a second #GtkStyleProperties
 * @replace: whether to replace values or not
 *
 * Merges into @props all the style information contained
 * in @props_to_merge. If @replace is %TRUE, the values
 * will be overwritten, if it is %FALSE, the older values
 * will prevail.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_merge (GtkStyleProperties       *props,
                            const GtkStyleProperties *props_to_merge,
                            gboolean                  replace)
{
  GtkStylePropertiesPrivate *priv, *priv_to_merge;
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props_to_merge));

  priv = props->priv;
  priv_to_merge = props_to_merge->priv;

  /* Merge symbolic color map */
  if (priv_to_merge->color_map)
    {
      g_hash_table_iter_init (&iter, priv_to_merge->color_map);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          const gchar *name;
          GtkSymbolicColor *color;

          name = key;
          color = value;

          if (!replace &&
              g_hash_table_lookup (priv->color_map, name))
            continue;

          gtk_style_properties_map_color (props, name, color);
        }
    }

  /* Merge symbolic style properties */
  g_hash_table_iter_init (&iter, priv_to_merge->properties);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      PropertyData *prop_to_merge = value;
      PropertyData *prop;
      guint i;

      prop = g_hash_table_lookup (priv->properties, key);

      if (!prop)
        {
          prop = property_data_new ();
          g_hash_table_insert (priv->properties, key, prop);
        }

      for (i = 0; i < prop_to_merge->values->len; i++)
        {
          ValueData *data;
          GValue *value;

          data = &g_array_index (prop_to_merge->values, ValueData, i);

          if (replace && data->state == GTK_STATE_FLAG_NORMAL &&
              G_VALUE_TYPE (&data->value) != PANGO_TYPE_FONT_DESCRIPTION)
            {
              /* Let normal state override all states
               * previously set in the original set
               */
              property_data_remove_values (prop);
            }

          value = property_data_get_value (prop, data->state);

          if (G_VALUE_TYPE (&data->value) == PANGO_TYPE_FONT_DESCRIPTION &&
              G_IS_VALUE (value))
            {
              PangoFontDescription *font_desc;
              PangoFontDescription *font_desc_to_merge;

              /* Handle merging of font descriptions */
              font_desc = g_value_get_boxed (value);
              font_desc_to_merge = g_value_get_boxed (&data->value);

              pango_font_description_merge (font_desc, font_desc_to_merge, replace);
            }
          else if (G_VALUE_TYPE (&data->value) == G_TYPE_PTR_ARRAY &&
                   G_IS_VALUE (value))
            {
              GPtrArray *array, *array_to_merge;
              gint i;

              /* Append the array, mainly thought
               * for the gtk-key-bindings property
               */
              array = g_value_get_boxed (value);
              array_to_merge = g_value_get_boxed (&data->value);

              for (i = 0; i < array_to_merge->len; i++)
                g_ptr_array_add (array, g_ptr_array_index (array_to_merge, i));
            }
          else if (replace || !G_IS_VALUE (value))
            {
              if (!G_IS_VALUE (value))
                g_value_init (value, G_VALUE_TYPE (&data->value));
              else if (G_VALUE_TYPE (value) != G_VALUE_TYPE (&data->value))
                {
                  g_value_unset (value);
                  g_value_init (value, G_VALUE_TYPE (&data->value));
                }

              g_value_copy (&data->value, value);
            }
        }
    }
}

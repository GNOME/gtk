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

#include <stdlib.h>
#include <gobject/gvaluecollector.h>

#include "gtkstyleprovider.h"
#include "gtkstyleset.h"
#include "gtkprivate.h"
#include "gtkthemingengine.h"
#include "gtkanimationdescription.h"
#include "gtkintl.h"

typedef struct GtkStyleSetPrivate GtkStyleSetPrivate;
typedef struct PropertyData PropertyData;
typedef struct PropertyNode PropertyNode;
typedef struct ValueData ValueData;

struct PropertyNode
{
  GQuark property_quark;
  GType property_type;
  GValue default_value;
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

struct GtkStyleSetPrivate
{
  GHashTable *color_map;
  GHashTable *properties;
};

static GArray *properties = NULL;

static void gtk_style_set_provider_init (GtkStyleProviderIface *iface);
static void gtk_style_set_finalize      (GObject      *object);


G_DEFINE_TYPE_EXTENDED (GtkStyleSet, gtk_style_set, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_style_set_provider_init));

static void
gtk_style_set_class_init (GtkStyleSetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GValue val = { 0 };

  object_class->finalize = gtk_style_set_finalize;

  /* Initialize default property set */
  gtk_style_set_register_property ("foreground-color", GDK_TYPE_COLOR, NULL, NULL);
  gtk_style_set_register_property ("background-color", GDK_TYPE_COLOR, NULL, NULL);
  gtk_style_set_register_property ("text-color", GDK_TYPE_COLOR, NULL, NULL);
  gtk_style_set_register_property ("base-color", GDK_TYPE_COLOR, NULL, NULL);

  gtk_style_set_register_property ("font", PANGO_TYPE_FONT_DESCRIPTION, NULL, NULL);

  gtk_style_set_register_property ("margin", GTK_TYPE_BORDER, NULL, NULL);
  gtk_style_set_register_property ("padding", GTK_TYPE_BORDER, NULL, NULL);
  gtk_style_set_register_property ("border", G_TYPE_INT, NULL, NULL);

  g_value_init (&val, GTK_TYPE_THEMING_ENGINE);
  g_value_set_object (&val, (GObject *) gtk_theming_engine_load (NULL));
  gtk_style_set_register_property ("engine", GTK_TYPE_THEMING_ENGINE, &val, NULL);
  g_value_unset (&val);

  g_value_init (&val, GTK_TYPE_ANIMATION_DESCRIPTION);
  g_value_take_boxed (&val, gtk_animation_description_new (0, GTK_TIMELINE_PROGRESS_LINEAR));
  gtk_style_set_register_property ("transition", GTK_TYPE_ANIMATION_DESCRIPTION, &val, NULL);
  g_value_unset (&val);

  g_type_class_add_private (object_class, sizeof (GtkStyleSetPrivate));
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
property_data_free (PropertyData *data)
{
  guint i;

  for (i = 0; i < data->values->len; i++)
    {
      ValueData *value_data;

      value_data = &g_array_index (data->values, ValueData, i);

      if (G_IS_VALUE (&value_data->value))
        g_value_unset (&value_data->value);
    }

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

      //val_data = &g_array_index (data->values, ValueData, pos);
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
gtk_style_set_init (GtkStyleSet *set)
{
  GtkStyleSetPrivate *priv;

  priv = set->priv = G_TYPE_INSTANCE_GET_PRIVATE (set,
                                                  GTK_TYPE_STYLE_SET,
                                                  GtkStyleSetPrivate);

  priv->properties = g_hash_table_new_full (NULL, NULL, NULL,
                                            (GDestroyNotify) property_data_free);
}

static void
gtk_style_set_finalize (GObject *object)
{
  GtkStyleSetPrivate *priv;
  GtkStyleSet *set;

  set = GTK_STYLE_SET (object);
  priv = set->priv;
  g_hash_table_destroy (priv->properties);

  if (priv->color_map)
    g_hash_table_destroy (priv->color_map);

  G_OBJECT_CLASS (gtk_style_set_parent_class)->finalize (object);
}

GtkStyleSet *
gtk_style_set_get_style (GtkStyleProvider *provider,
                         GtkWidgetPath    *path)
{
  /* Return style set itself */
  return g_object_ref (provider);
}

static void
gtk_style_set_provider_init (GtkStyleProviderIface *iface)
{
  iface->get_style = gtk_style_set_get_style;
}

static int
compare_property (gconstpointer p1,
                  gconstpointer p2)
{
  PropertyNode *key = (PropertyNode *) p1;
  PropertyNode *node = (PropertyNode *) p2;

  return (int) key->property_quark - node->property_quark;
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
void
gtk_style_set_register_property (const gchar            *property_name,
                                 GType                   type,
                                 const GValue           *default_value,
                                 GtkStylePropertyParser  parse_func)
{
  PropertyNode *node, new = { 0 };
  GQuark quark;
  gint i;

  g_return_if_fail (property_name != NULL);
  g_return_if_fail (type != 0);

  if (G_UNLIKELY (!properties))
    properties = g_array_new (FALSE, TRUE, sizeof (PropertyNode));

  quark = g_quark_try_string (property_name);

  if ((node = property_node_lookup (quark)) != NULL)
    {
      g_warning ("Property \"%s\" was already registered with type %s",
                 property_name, g_type_name (node->property_type));
      return;
    }

  quark = g_quark_from_string (property_name);

  new.property_quark = quark;
  new.property_type = type;

  if (default_value)
    {
      g_value_init (&new.default_value, G_VALUE_TYPE (default_value));
      g_value_copy (default_value, &new.default_value);
    }
  else
    g_value_init (&new.default_value, type);

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

gboolean
gtk_style_set_lookup_property (const gchar            *property_name,
                               GType                  *type,
                               GtkStylePropertyParser *parse_func)
{
  PropertyNode *node;
  GtkStyleSetClass *klass;
  gboolean found = FALSE;
  GQuark quark;
  gint i;

  g_return_val_if_fail (property_name != NULL, FALSE);

  klass = g_type_class_ref (GTK_TYPE_STYLE_SET);
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
          if (type)
            *type = node->property_type;

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

/* GtkStyleSet methods */

GtkStyleSet *
gtk_style_set_new (void)
{
  return g_object_new (GTK_TYPE_STYLE_SET, NULL);
}

void
gtk_style_set_map_color (GtkStyleSet      *set,
			 const gchar      *name,
			 GtkSymbolicColor *color)
{
  GtkStyleSetPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (name != NULL);
  g_return_if_fail (color != NULL);

  priv = set->priv;

  if (G_UNLIKELY (!priv->color_map))
    priv->color_map = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             (GDestroyNotify) g_free,
                                             (GDestroyNotify) gtk_symbolic_color_unref);

  g_hash_table_replace (priv->color_map,
                        g_strdup (name),
                        gtk_symbolic_color_ref (color));
}

GtkSymbolicColor *
gtk_style_set_lookup_color (GtkStyleSet *set,
			    const gchar *name)
{
  GtkStyleSetPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_SET (set), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  priv = set->priv;

  if (!priv->color_map)
    return NULL;

  return g_hash_table_lookup (priv->color_map, name);
}

void
gtk_style_set_set_property (GtkStyleSet   *set,
                            const gchar   *property,
                            GtkStateFlags  state,
                            const GValue  *value)
{
  GtkStyleSetPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;
  GType value_type;
  GValue *val;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  value_type = G_VALUE_TYPE (value);
  node = property_node_lookup (g_quark_try_string (property));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }

  if (node->property_type == GDK_TYPE_COLOR)
    {
      /* Allow GtkSymbolicColor as well */
      g_return_if_fail (value_type == GDK_TYPE_COLOR || value_type == GTK_TYPE_SYMBOLIC_COLOR);
    }
  else
    g_return_if_fail (node->property_type == value_type);

  priv = set->priv;
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

void
gtk_style_set_set_valist (GtkStyleSet   *set,
                          GtkStateFlags  state,
                          va_list        args)
{
  GtkStyleSetPrivate *priv;
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_SET (set));

  priv = set->priv;
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

      g_value_init (val, node->property_type);
      G_VALUE_COLLECT (val, args, 0, &error);

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

void
gtk_style_set_set (GtkStyleSet   *set,
                   GtkStateFlags  state,
                   ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_SET (set));

  va_start (args, state);
  gtk_style_set_set_valist (set, state, args);
  va_end (args);
}

static gboolean
resolve_color (GtkStyleSet *set,
	       GValue      *value)
{
  GdkColor color;

  /* Resolve symbolic color to GdkColor */
  if (!gtk_symbolic_color_resolve (g_value_get_boxed (value), set, &color))
    return FALSE;

  /* Store it back, this is where GdkColor caching happens */
  g_value_unset (value);
  g_value_init (value, GDK_TYPE_COLOR);
  g_value_set_boxed (value, &color);

  return TRUE;
}

gboolean
gtk_style_set_get_property (GtkStyleSet   *set,
                            const gchar   *property,
                            GtkStateFlags  state,
                            GValue        *value)
{
  GtkStyleSetPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;
  GValue *val;

  g_return_val_if_fail (GTK_IS_STYLE_SET (set), FALSE);
  g_return_val_if_fail (property != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  node = property_node_lookup (g_quark_try_string (property));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return FALSE;
    }

  priv = set->priv;
  prop = g_hash_table_lookup (priv->properties,
                              GINT_TO_POINTER (node->property_quark));

  if (!prop)
    return FALSE;

  g_value_init (value, node->property_type);

  val = property_data_match_state (prop, state);

  if (!val)
    val = &node->default_value;

  g_return_val_if_fail (G_IS_VALUE (val), FALSE);

  if (G_VALUE_TYPE (val) == GTK_TYPE_SYMBOLIC_COLOR)
    {
      g_return_val_if_fail (node->property_type == GDK_TYPE_COLOR, FALSE);

      if (!resolve_color (set, val))
        return FALSE;
    }

  g_value_copy (val, value);

  return TRUE;
}

void
gtk_style_set_get_valist (GtkStyleSet   *set,
                          GtkStateFlags  state,
                          va_list        args)
{
  GtkStyleSetPrivate *priv;
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_SET (set));

  priv = set->priv;
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

      if (!val)
        val = &node->default_value;

      if (G_VALUE_TYPE (val) == GTK_TYPE_SYMBOLIC_COLOR)
        {
          g_return_if_fail (node->property_type == GDK_TYPE_COLOR);

          if (!resolve_color (set, val))
            val = &node->default_value;
        }

      G_VALUE_LCOPY (val, args, 0, &error);

      if (error)
        {
          g_warning ("Could not get style property \"%s\": %s", property_name, error);
          g_free (error);
          break;
        }

      property_name = va_arg (args, const gchar *);
    }
}

void
gtk_style_set_get (GtkStyleSet   *set,
                   GtkStateFlags  state,
                   ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_SET (set));

  va_start (args, state);
  gtk_style_set_get_valist (set, state, args);
  va_end (args);
}

void
gtk_style_set_unset_property (GtkStyleSet   *set,
                              const gchar   *property,
                              GtkStateFlags  state)
{
  GtkStyleSetPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;
  guint pos;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (property != NULL);

  node = property_node_lookup (g_quark_try_string (property));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }

  priv = set->priv;
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

void
gtk_style_set_clear (GtkStyleSet *set)
{
  GtkStyleSetPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_SET (set));

  priv = set->priv;
  g_hash_table_remove_all (priv->properties);
}

void
gtk_style_set_merge (GtkStyleSet       *set,
                     const GtkStyleSet *set_to_merge,
                     gboolean           replace)
{
  GtkStyleSetPrivate *priv, *priv_to_merge;
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (GTK_IS_STYLE_SET (set_to_merge));

  priv = set->priv;
  priv_to_merge = set_to_merge->priv;

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

          gtk_style_set_map_color (set, name, color);
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
          value = property_data_get_value (prop, data->state);

          if (replace || !G_IS_VALUE (value))
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

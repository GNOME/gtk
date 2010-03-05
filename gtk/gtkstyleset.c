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
#include "gtkintl.h"

#include "gtkalias.h"

typedef struct GtkStyleSetPrivate GtkStyleSetPrivate;
typedef struct PropertyData PropertyData;
typedef struct PropertyNode PropertyNode;

struct PropertyNode
{
  GQuark property_quark;
  GType property_type;
  GValue default_value;
};

struct PropertyData
{
  GValue values[GTK_STATE_LAST];
};

struct GtkStyleSetPrivate
{
  GHashTable *properties;
};

static GArray *properties = NULL;

#define GTK_STYLE_SET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_STYLE_SET, GtkStyleSetPrivate))

static void gtk_style_set_provider_init (GtkStyleProviderIface *iface);
static void gtk_style_set_finalize      (GObject      *object);


G_DEFINE_TYPE_EXTENDED (GtkStyleSet, gtk_style_set, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_style_set_provider_init));

static void
gtk_style_set_class_init (GtkStyleSetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkColor black = { 0, 0, 0, 0 };
  GdkColor white = { 0, 65535, 65535, 65535 };

  object_class->finalize = gtk_style_set_finalize;

  /* Initialize default property set */
  gtk_style_set_register_property_color ("foreground-color", &white);
  gtk_style_set_register_property_color ("background-color", &black);
  gtk_style_set_register_property_color ("text-color", &white);
  gtk_style_set_register_property_color ("base-color", &white);

  g_type_class_add_private (object_class, sizeof (GtkStyleSetPrivate));
}

static PropertyData *
property_data_new (void)
{
  PropertyData *data;

  data = g_slice_new0 (PropertyData);

  return data;
}

static void
property_data_free (PropertyData *data)
{
  gint i;

  for (i = 0; i <= GTK_STATE_INSENSITIVE; i++)
    {
      if (G_IS_VALUE (&data->values[i]))
        g_value_unset (&data->values[i]);
    }

  g_slice_free (PropertyData, data);
}

static void
gtk_style_set_init (GtkStyleSet *set)
{
  GtkStyleSetPrivate *priv;

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  priv->properties = g_hash_table_new_full (NULL, NULL, NULL,
                                            (GDestroyNotify) property_data_free);
}

static void
gtk_style_set_finalize (GObject *object)
{
  GtkStyleSetPrivate *priv;

  priv = GTK_STYLE_SET_GET_PRIVATE (object);
  g_hash_table_destroy (priv->properties);

  G_OBJECT_CLASS (gtk_style_set_parent_class)->finalize (object);
}

GtkStyleSet *
gtk_style_set_get_style (GtkStyleProvider *provider)
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
gtk_style_set_register_property (const gchar *property_name,
                                 GType        type,
                                 GValue      *default_value)
{
  PropertyNode *node, new = { 0 };
  GQuark quark;
  gint i;

  g_return_if_fail (property_name != NULL);
  g_return_if_fail (default_value != NULL);
  g_return_if_fail (type == G_VALUE_TYPE (default_value));

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

  g_value_init (&new.default_value, type);
  g_value_copy (default_value, &new.default_value);

  for (i = 0; i < properties->len; i++)
    {
      node = &g_array_index (properties, PropertyNode, i);

      if (node->property_quark > quark)
        break;
    }

  g_array_insert_val (properties, i, new);
}

void
gtk_style_set_register_property_color (const gchar *property_name,
                                       GdkColor    *initial_value)
{
  GValue value = { 0 };

  g_return_if_fail (property_name != NULL);
  g_return_if_fail (initial_value != NULL);

  g_value_init (&value, GDK_TYPE_COLOR);
  g_value_set_boxed (&value, initial_value);

  gtk_style_set_register_property (property_name, GDK_TYPE_COLOR, &value);

  g_value_unset (&value);
}

void
gtk_style_set_register_property_int (const gchar *property_name,
                                     gint         initial_value)
{
  GValue value = { 0 };

  g_return_if_fail (property_name != NULL);

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, initial_value);

  gtk_style_set_register_property (property_name, G_TYPE_INT, &value);

  g_value_unset (&value);
}

void
gtk_style_set_register_property_uint (const gchar *property_name,
                                      guint        initial_value)
{
  GValue value = { 0 };

  g_return_if_fail (property_name != NULL);

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, initial_value);

  gtk_style_set_register_property (property_name, G_TYPE_UINT, &value);

  g_value_unset (&value);
}

void
gtk_style_set_register_property_double (const gchar *property_name,
                                        gdouble      initial_value)
{
  GValue value = { 0 };

  g_return_if_fail (property_name != NULL);

  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, initial_value);

  gtk_style_set_register_property (property_name, G_TYPE_DOUBLE, &value);

  g_value_unset (&value);
}

/* GtkStyleSet methods */

GtkStyleSet *
gtk_style_set_new (void)
{
  return g_object_new (GTK_TYPE_STYLE_SET, NULL);
}

void
gtk_style_set_set_property (GtkStyleSet  *set,
                            const gchar  *property,
                            GtkStateType  state,
                            const GValue *value)
{
  GtkStyleSetPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (property != NULL);
  g_return_if_fail (state < GTK_STATE_LAST);
  g_return_if_fail (value != NULL);

  node = property_node_lookup (g_quark_try_string (property));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }

  g_return_if_fail (node->property_type == G_VALUE_TYPE (value));

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  prop = g_hash_table_lookup (priv->properties,
                              GINT_TO_POINTER (node->property_quark));

  if (!prop)
    {
      prop = property_data_new ();
      g_hash_table_insert (priv->properties,
                           GINT_TO_POINTER (node->property_quark),
                           prop);
    }

  if (G_IS_VALUE (&prop->values[state]))
    g_value_reset (&prop->values[state]);
  else
    g_value_init (&prop->values[state], node->property_type);

  g_value_copy (value, &prop->values[state]);
}


void
gtk_style_set_set_valist (GtkStyleSet  *set,
                          GtkStateType  state,
                          va_list       args)
{
  GtkStyleSetPrivate *priv;
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (state < GTK_STATE_LAST);

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      PropertyNode *node;
      PropertyData *prop;
      gchar *error = NULL;

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

      g_value_init (&prop->values[state], node->property_type);
      G_VALUE_COLLECT (&prop->values[state], args, 0, &error);

      if (error)
        {
          g_warning ("Could not set style property \"%s\": %s", property_name, error);
          g_value_unset (&prop->values[state]);
	  g_free (error);
          break;
        }

      property_name = va_arg (args, const gchar *);
    }
}

void
gtk_style_set_set (GtkStyleSet  *set,
                   GtkStateType  state,
                   ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (state < GTK_STATE_LAST);

  va_start (args, state);
  gtk_style_set_set_valist (set, state, args);
  va_end (args);
}

gboolean
gtk_style_set_get_property (GtkStyleSet  *set,
                            const gchar  *property,
                            GtkStateType  state,
                            GValue       *value)
{
  GtkStyleSetPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;

  g_return_val_if_fail (GTK_IS_STYLE_SET (set), FALSE);
  g_return_val_if_fail (property != NULL, FALSE);
  g_return_val_if_fail (state < GTK_STATE_LAST, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  node = property_node_lookup (g_quark_try_string (property));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return FALSE;
    }

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  prop = g_hash_table_lookup (priv->properties,
                              GINT_TO_POINTER (node->property_quark));

  if (!prop)
    return FALSE;

  g_value_init (value, G_VALUE_TYPE (&prop->values[state]));
  g_value_copy (&prop->values[state], value);

  return TRUE;
}

void
gtk_style_set_get_valist (GtkStyleSet  *set,
                          GtkStateType  state,
                          va_list       args)
{
  GtkStyleSetPrivate *priv;
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (state < GTK_STATE_LAST);

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      PropertyNode *node;
      PropertyData *prop;
      gchar *error = NULL;

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
          /* FIXME: Fill in default */
          break;
        }

      G_VALUE_LCOPY (&prop->values[state], args, 0, &error);

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
gtk_style_set_get (GtkStyleSet  *set,
                   GtkStateType  state,
                   ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (state < GTK_STATE_LAST);

  va_start (args, state);
  gtk_style_set_get_valist (set, state, args);
  va_end (args);
}

void
gtk_style_set_unset_property (GtkStyleSet  *set,
                              const gchar  *property,
                              GtkStateType  state)
{
  GtkStyleSetPrivate *priv;
  PropertyNode *node;
  PropertyData *prop;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (property != NULL);
  g_return_if_fail (state < GTK_STATE_LAST);

  node = property_node_lookup (g_quark_try_string (property));

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  prop = g_hash_table_lookup (priv->properties,
                              GINT_TO_POINTER (node->property_quark));

  if (!prop)
    return;

  g_value_unset (&prop->values[state]);
}

void
gtk_style_set_clear (GtkStyleSet *set)
{
  GtkStyleSetPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_SET (set));

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
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

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  priv_to_merge = GTK_STYLE_SET_GET_PRIVATE (set_to_merge);

  g_hash_table_iter_init (&iter, priv_to_merge->properties);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      PropertyData *prop_to_merge = value;
      PropertyData *prop = NULL;
      GtkStateType i;

      for (i = GTK_STATE_NORMAL; i < GTK_STATE_LAST; i++)
        {
          if (G_VALUE_TYPE (&prop_to_merge->values[i]) == G_TYPE_INVALID)
            continue;

          if (!prop)
            prop = g_hash_table_lookup (priv->properties, key);

          if (!prop)
            {
              prop = property_data_new ();
              g_hash_table_insert (priv->properties, key, prop);
            }

          if (replace ||
              G_VALUE_TYPE (&prop->values[i]) == G_TYPE_INVALID)
            {
              if (G_VALUE_TYPE (&prop->values[i]) == G_TYPE_INVALID)
                g_value_init (&prop->values[i], G_VALUE_TYPE (&prop_to_merge->values[i]));

              g_value_copy (&prop_to_merge->values[i],
                            &prop->values[i]);
            }
        }
    }
}

#define __GTK_STYLE_SET_C__
#include "gtkaliasdef.c"

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

#include "gtkstyleset.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include "gtkalias.h"

typedef struct GtkStyleSetPrivate GtkStyleSetPrivate;
typedef struct PropertyData PropertyData;

struct PropertyData
{
  GValue values[GTK_STATE_LAST];
};

struct GtkStyleSetPrivate
{
  GHashTable *properties;
};

#define GTK_STYLE_SET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_STYLE_SET, GtkStyleSetPrivate))

static void gtk_style_set_finalize     (GObject      *object);


G_DEFINE_TYPE (GtkStyleSet, gtk_style_set, G_TYPE_OBJECT)

static void
gtk_style_set_class_init (GtkStyleSetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_set_finalize;

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
    g_value_unset (&data->values[i]);

  g_slice_free (PropertyData, data);
}

static void
gtk_style_set_init (GtkStyleSet *set)
{
  GtkStyleSetPrivate *priv;

  priv = GTK_STYLE_SET_GET_PRIVATE (set);

  priv->properties = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            (GDestroyNotify) g_free,
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
  PropertyData *prop;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (property != NULL);
  g_return_if_fail (state < GTK_STATE_LAST);
  g_return_if_fail (value != NULL);

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  prop = g_hash_table_lookup (priv->properties, property);

  if (!prop)
    {
      prop = property_data_new ();
      g_hash_table_insert (priv->properties,
                           g_strdup (property),
                           prop);
    }

  if (G_VALUE_TYPE (&prop->values[state]) != G_TYPE_INVALID)
    g_value_unset (&prop->values[state]);

  g_value_init (&prop->values[state], G_VALUE_TYPE (value));
  g_value_copy (value, &prop->values[state]);
}

gboolean
gtk_style_set_get_property (GtkStyleSet  *set,
                            const gchar  *property,
                            GtkStateType  state,
                            GValue       *value)
{
  GtkStyleSetPrivate *priv;
  PropertyData *prop;

  g_return_val_if_fail (GTK_IS_STYLE_SET (set), FALSE);
  g_return_val_if_fail (property != NULL, FALSE);
  g_return_val_if_fail (state < GTK_STATE_LAST, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  prop = g_hash_table_lookup (priv->properties, property);

  if (!prop)
    return FALSE;

  g_value_init (value, G_VALUE_TYPE (&prop->values[state]));
  g_value_copy (&prop->values[state], value);

  return TRUE;
}

void
gtk_style_set_unset_property (GtkStyleSet  *set,
                              const gchar  *property,
                              GtkStateType  state)
{
  GtkStyleSetPrivate *priv;
  PropertyData *prop;

  g_return_if_fail (GTK_IS_STYLE_SET (set));
  g_return_if_fail (property != NULL);
  g_return_if_fail (state < GTK_STATE_LAST);

  priv = GTK_STYLE_SET_GET_PRIVATE (set);
  prop = g_hash_table_lookup (priv->properties, property);

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
      const gchar *name_to_merge = key;
      PropertyData *prop_to_merge = value;
      PropertyData *prop = NULL;
      GtkStateType i;

      for (i = GTK_STATE_NORMAL; i < GTK_STATE_LAST; i++)
        {
          if (G_VALUE_TYPE (&prop_to_merge->values[i]) == G_TYPE_INVALID)
            continue;

          if (!prop)
            prop = g_hash_table_lookup (priv->properties, name_to_merge);

          if (!prop)
            {
              prop = property_data_new ();
              g_hash_table_insert (priv->properties,
                                   g_strdup (name_to_merge),
                                   prop);
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

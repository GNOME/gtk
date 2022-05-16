/*
 * Copyright © 2022 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkinvertiblesorter.h"

#include "gtkintl.h"
#include "gtksorterprivate.h"
#include "gtktypebuiltins.h"

#include <math.h>

/**
 * GtkInvertibleSorter:
 *
 * `GtkInvertibleSorter` is a `GtkSorter` that can invert
 * the order produced by a sorter.
 */

struct _GtkInvertibleSorter
{
  GtkSorter parent_instance;

  GtkSorter *sorter;
  GtkSortType sort_order;
};

enum {
  PROP_0,
  PROP_SORTER,
  PROP_SORT_ORDER,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkInvertibleSorter, gtk_invertible_sorter, GTK_TYPE_SORTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void
sorter_changed (GtkSorter *sorter, int change, gpointer data)
{
  gtk_sorter_changed (GTK_SORTER (data), GTK_SORTER_CHANGE_DIFFERENT);
}

static GtkOrdering
gtk_invertible_sorter_compare (GtkSorter *sorter,
                               gpointer   item1,
                               gpointer   item2)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (sorter);
  GtkOrdering result;

  result = gtk_sorter_compare (self->sorter, item1, item2);

  if (self->sort_order == GTK_SORT_DESCENDING)
    result = - result;

  return result;
}

static GtkSorterOrder
gtk_invertible_sorter_get_order (GtkSorter *sorter)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (sorter);

  return gtk_sorter_get_order (self->sorter);
}

static void
gtk_invertible_sorter_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (object);

  switch (prop_id)
    {
    case PROP_SORTER:
      {
        GtkSorter *sorter;

        sorter = g_value_get_object (value);
        g_set_object (&self->sorter, sorter);
        g_signal_connect (sorter, "changed", G_CALLBACK (sorter_changed), self);
      }
      break;

    case PROP_SORT_ORDER:
      gtk_invertible_sorter_set_sort_order (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_invertible_sorter_get_property (GObject     *object,
                                    guint        prop_id,
                                    GValue      *value,
                                    GParamSpec  *pspec)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (object);

  switch (prop_id)
    {
    case PROP_SORTER:
      g_value_set_object (value, self->sorter);
      break;

    case PROP_SORT_ORDER:
      g_value_set_enum (value, self->sort_order);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_invertible_sorter_dispose (GObject *object)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (object);

  g_signal_handlers_disconnect_by_func (self->sorter, sorter_changed, self);
  g_clear_object (&self->sorter);

  G_OBJECT_CLASS (gtk_invertible_sorter_parent_class)->dispose (object);
}

static void
gtk_invertible_sorter_class_init (GtkInvertibleSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_invertible_sorter_compare;
  sorter_class->get_order = gtk_invertible_sorter_get_order;

  object_class->get_property = gtk_invertible_sorter_get_property;
  object_class->set_property = gtk_invertible_sorter_set_property;
  object_class->dispose = gtk_invertible_sorter_dispose;

  /**
   * GtkNumericSorter:sorter: (type GtkSorter) (attributes org.gtk.Property.get=gtk_invertible_sorter_get_sorter)
   *
   * The sorter.
   */
  properties[PROP_SORTER] =
    g_param_spec_object ("sorter", NULL, NULL,
                         GTK_TYPE_SORTER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkInvertibleSorter:sort-order: (attributes org.gtk.Property.get=gtk_invertible_sorter_get_sort_order org.gtk.Property.set=gtk_invertible_sorter_set_sort_order)
   *
   * Whether the sorter will sort smaller items first.
   */
  properties[PROP_SORT_ORDER] =
    g_param_spec_enum ("sort-order", NULL, NULL,
                       GTK_TYPE_SORT_TYPE,
                       GTK_SORT_ASCENDING,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

static void
gtk_invertible_sorter_init (GtkInvertibleSorter *self)
{
  self->sort_order = GTK_SORT_ASCENDING;

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_INVERTED);
}

/**
 * gtk_invertible_sorter_new:
 * @sorter: (transfer full): The sorter to use
 *
 * Creates a new invertible sorter using the given @sorter.
 *
 * Returns: a new `GtkInvertibleSorter`
 *
 * Since: 4.8
 */
GtkInvertibleSorter *
gtk_invertible_sorter_new (GtkSorter   *sorter,
                           GtkSortType  sort_order)
{
  GtkInvertibleSorter *result;

  result = g_object_new (GTK_TYPE_INVERTIBLE_SORTER,
                         "sorter", sorter,
                         "sort-order", sort_order,
                         NULL);

  g_clear_object (&sorter);

  return result;
}

/**
 * gtk_invertible_sorter_get_sorter: (attributes org.gtk.Method.get_property=sorter)
 * @self: a `GtkInvertibleSorter`
 *
 * Gets the sorter.
 *
 * Returns: (transfer none): the `GtkSorter`
 *
 * Since: 4.8
 */
GtkSorter *
gtk_invertible_sorter_get_sorter (GtkInvertibleSorter *self)
{
  g_return_val_if_fail (GTK_IS_INVERTIBLE_SORTER (self), NULL);

  return self->sorter;
}

/**
 * gtk_invertible_sorter_set_sort_order: (attributes org.gtk.Method.set_property=sort-order)
 * @self: a `GtkInvertibleSorter`
 * @sort_order: whether to sort smaller items first
 *
 * Sets whether to sort smaller items before larger ones.
 *
 * Since: 4.8
 */
void
gtk_invertible_sorter_set_sort_order (GtkInvertibleSorter *self,
                                      GtkSortType          sort_order)
{
  g_return_if_fail (GTK_IS_INVERTIBLE_SORTER (self));

  if (self->sort_order == sort_order)
    return;

  self->sort_order = sort_order;

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_INVERTED);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORT_ORDER]);
}

/**
 * gtk_invertible_sorter_get_sort_order: (attributes org.gtk.Method.get_property=sort-order)
 * @self: a `GtkInvertibleSorter`
 *
 * Gets whether this sorter will sort smaller items first.
 *
 * Returns: the order of the items
 *
 * Since: 4.8
 */
GtkSortType
gtk_invertible_sorter_get_sort_order (GtkInvertibleSorter *self)
{
  g_return_val_if_fail (GTK_IS_INVERTIBLE_SORTER (self), GTK_SORT_ASCENDING);

  return self->sort_order;
}

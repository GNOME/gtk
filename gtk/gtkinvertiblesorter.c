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

#include "gtksorterprivate.h"
#include "gtktypebuiltins.h"

/**
 * GtkInvertibleSorter:
 *
 * `GtkInvertibleSorter` wraps another sorter and
 * makes it possible to invert its order.
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

  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };


G_DEFINE_TYPE (GtkInvertibleSorter, gtk_invertible_sorter, GTK_TYPE_SORTER)

static GtkOrdering
gtk_invertible_sorter_compare (GtkSorter *sorter,
                               gpointer   item1,
                               gpointer   item2)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (sorter);
  GtkOrdering result = GTK_ORDERING_EQUAL;

  if (self->sorter)
    result = gtk_sorter_compare (self->sorter, item1, item2);

  if (self->sort_order == GTK_SORT_DESCENDING)
    result = -result;

  return result;
}

static GtkSorterOrder
gtk_invertible_sorter_get_order (GtkSorter *sorter)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (sorter);

  if (self->sorter)
    return gtk_sorter_get_order (self->sorter);

  return GTK_SORTER_ORDER_NONE;
}

static void
gtk_invertible_sorter_changed_cb (GtkSorter           *sorter,
                                  GtkSorterChange      change,
                                  GtkInvertibleSorter *self)
{
  gtk_sorter_changed (GTK_SORTER (self), change);
}

static void
gtk_invertible_sorter_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
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
gtk_invertible_sorter_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (object);

  switch (prop_id)
    {
    case PROP_SORTER:
      gtk_invertible_sorter_set_sorter (self, g_value_get_object (value));
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
gtk_invertible_sorter_dispose (GObject *object)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (object);

  if (self->sorter)
    g_signal_handlers_disconnect_by_func (self->sorter, G_CALLBACK (gtk_invertible_sorter_changed_cb), self);
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
   * GtkInvertibleSorter:sorter: (attributes org.gtk.Property.get=gtk_invertible_sorter_get_sorter org.gtk.Property.set=gtk_invertible_sorter_set_sorter)
   *
   * The sorter.
   *
   * Since: 4.10
   **/
  properties[PROP_SORTER] =
    g_param_spec_object ("sorter", NULL, NULL,
                         GTK_TYPE_SORTER,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkInvertibleSorter:sort-order: (attributes org.gtk.Property.get=gtk_invertible_sorter_get_sort_order org.gtk.Property.set=gtk_invertible_sorter_set_sort_order)
   *
   * Whether the order of the underlying sorter is inverted.
   *
   * The order of the underlying sorter is considered ascending.
   *
   * To make the invertible sorter invert the order,
   * set this property to `GTK_SORT_DESCENDING`.
   *
   * Since: 4.10
   **/
  properties[PROP_SORT_ORDER] =
    g_param_spec_enum ("sort-order", NULL, NULL,
                       GTK_TYPE_SORT_TYPE,
                       GTK_SORT_ASCENDING,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_invertible_sorter_init (GtkInvertibleSorter *self)
{
}

/**
 * gtk_invertible_sorter_new:
 * @sorter: (nullable) (transfer full): the sorter
 *
 * Creates a new invertible sorter.
 *
 * This sorter compares items like @sorter, optionally
 * inverting the order.
 *
 * Returns: a new `GtkInvertibleSorter`
 *
 * Since: 4.10
 */
GtkInvertibleSorter *
gtk_invertible_sorter_new (GtkSorter *sorter)
{
  GtkInvertibleSorter *self;

  self = g_object_new (GTK_TYPE_INVERTIBLE_SORTER,
                       "sorter", sorter,
                       NULL);
  g_clear_object (&sorter);

  return self;
}

/**
 * gtk_invertible_sorter_set_sorter:
 * @self: a `GtkInvertibleSorter`
 * @sorter: (nullable): a sorter
 *
 * Sets the sorter.
 *
 * Since: 4.10
 */
void
gtk_invertible_sorter_set_sorter (GtkInvertibleSorter *self,
                                  GtkSorter           *sorter)
{
  g_return_if_fail (GTK_IS_INVERTIBLE_SORTER (self));
  g_return_if_fail (sorter == NULL || GTK_IS_SORTER (sorter));

  if (self->sorter)
    g_signal_handlers_disconnect_by_func (self->sorter, G_CALLBACK (gtk_invertible_sorter_changed_cb), self);

  if (sorter)
    g_signal_connect (sorter, "changed", G_CALLBACK (gtk_invertible_sorter_changed_cb), self);

  if (g_set_object (&self->sorter, sorter))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORTER]);
      gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);
    }
}

/**
 * gtk_invertible_sorter_get_sorter:
 * @self: a `GtkInvertibleSorter`
 *
 * Returns the sorter.
 *
 * Returns: (nullable) (transfer none): the sorter
 *
 * Since: 4.10
 */
GtkSorter *
gtk_invertible_sorter_get_sorter (GtkInvertibleSorter *self)
{
  g_return_val_if_fail (GTK_IS_INVERTIBLE_SORTER (self), NULL);

  return self->sorter;
}

/**
 * gtk_invertible_sorter_set_sort_order:
 * @self: a `GtkInvertibleSorter`
 * @sort_order: the new sort order
 *
 * Sets whether to invert the order of the wrapped sorter.
 *
 * Since: 4.10
 */
void
gtk_invertible_sorter_set_sort_order (GtkInvertibleSorter *self,
                                      GtkSortType          sort_order)
{
  g_return_if_fail (GTK_IS_INVERTIBLE_SORTER (self));

  if (self->sort_order == sort_order)
    return;

  self->sort_order = sort_order;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORT_ORDER]);
  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_INVERTED);
}

/**
 * gtk_invertible_sorter_get_sort_order:
 * @self: a `GtkInvertibleSorter`
 *
 * Returns the sort order of @self.
 *
 * If the sort order is `GTK_SORT_DESCENDING`,
 * then the underlying order is inverted.
 *
 * Returns: whether the order is inverted
 *
 * Since: 4.10
 */
GtkSortType
gtk_invertible_sorter_get_sort_order (GtkInvertibleSorter *self)
{
  g_return_val_if_fail (GTK_IS_INVERTIBLE_SORTER (self), GTK_SORT_ASCENDING);

  return self->sort_order;
}

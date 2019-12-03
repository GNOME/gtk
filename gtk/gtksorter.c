/*
 * Copyright © 2019 Matthias Clasen
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

#include "gtksorter.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

typedef struct _GtkSorterPrivate GtkSorterPrivate;
struct _GtkSorterPrivate 
{
  GtkSortType sort_direction;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_SORT_DIRECTION = 1,
  NUM_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSorter, gtk_sorter, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *props[NUM_PROPERTIES];

static int
gtk_sorter_default_compare (GtkSorter *self,
                            gpointer   item1,
                            gpointer   item2)
{
  g_critical ("Sorter of type '%s' does not implement GtkSorter::compare", G_OBJECT_TYPE_NAME (self));

  return 0;
}

static void
gtk_sorter_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkSorter *sorter = GTK_SORTER (object);

  switch (prop_id)
    {
    case PROP_SORT_DIRECTION:
      gtk_sorter_set_sort_direction (sorter, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_sorter_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GtkSorter *sorter = GTK_SORTER (object);

  switch (prop_id)
    {
    case PROP_SORT_DIRECTION:
      g_value_set_enum (value, gtk_sorter_get_sort_direction (sorter));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_sorter_class_init (GtkSorterClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_sorter_set_property;
  gobject_class->get_property = gtk_sorter_get_property;

  class->compare = gtk_sorter_default_compare;

  props[PROP_SORT_DIRECTION] =
    g_param_spec_enum ("sort-direction",
                       P_("Sort direction"),
                       P_("Whether to sort ascending or descending"),
                       GTK_TYPE_SORT_TYPE,
                       GTK_SORT_ASCENDING,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, props);

  /**
   * GtkSearch:changed:
   * @self: The #GtkSorter
   *
   * This signal is emitted whenever the sorter changed. Users of the sorter
   * should then check items again via gtk_sorter_compare().
   */
  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals[CHANGED],
                              G_TYPE_FROM_CLASS (gobject_class),
                              g_cclosure_marshal_VOID__VOIDv);
}

static void
gtk_sorter_init (GtkSorter *self)
{
}

int
gtk_sorter_compare (GtkSorter *self,
                    gpointer   item1,
                    gpointer   item2)
{
  GtkSorterPrivate *priv = gtk_sorter_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_SORTER (self), 0);
  g_return_val_if_fail (item1 && item2, 0);

  if (priv->sort_direction == GTK_SORT_ASCENDING)
    return GTK_SORTER_GET_CLASS (self)->compare (self, item1, item2);
  else
    return - GTK_SORTER_GET_CLASS (self)->compare (self, item1, item2);
}

void
gtk_sorter_set_sort_direction (GtkSorter   *self,
                               GtkSortType  direction)
{
  GtkSorterPrivate *priv = gtk_sorter_get_instance_private (self);

  g_return_if_fail (GTK_IS_SORTER (self));

  if (priv->sort_direction == direction)
    return;

  priv->sort_direction = direction;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SORT_DIRECTION]);

  g_signal_emit (self, signals[CHANGED], 0); 
}

GtkSortType
gtk_sorter_get_sort_direction (GtkSorter *self)
{
  GtkSorterPrivate *priv = gtk_sorter_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_SORTER (self), GTK_SORT_ASCENDING);

  return priv->sort_direction;
}

void
gtk_sorter_changed (GtkSorter *self)
{
  g_signal_emit (self, signals[CHANGED], 0); 
}

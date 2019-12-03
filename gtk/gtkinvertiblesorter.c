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

#include "gtkinvertiblesorter.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

struct _GtkInvertibleSorter
{
  GtkSorter parent_instance;

  GtkSortType direction;

  GtkSorter *sorter;
};

enum {
  PROP_0,
  PROP_SORTER,
  PROP_DIRECTION,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkInvertibleSorter, gtk_invertible_sorter, GTK_TYPE_SORTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static int
gtk_invertible_sorter_compare (GtkSorter *sorter,
                               gpointer   item1,
                               gpointer   item2)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (sorter);
  int result;

  if (self->sorter == NULL)
    return 0;

  result = gtk_sorter_compare (self->sorter, item1, item2);

  if (self->direction == GTK_SORT_ASCENDING)
    return result;
  else
    return - result;
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

    case PROP_DIRECTION:
      gtk_invertible_sorter_set_direction (self, g_value_get_enum (value));
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

    case PROP_DIRECTION:
      g_value_set_enum (value, self->direction);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void changed_cb (GtkSorter *sorter, int change, gpointer data);

static void
gtk_invertible_sorter_dispose (GObject *object)
{
  GtkInvertibleSorter *self = GTK_INVERTIBLE_SORTER (object);

  if (self->sorter)
    g_signal_handlers_disconnect_by_func (self->sorter, changed_cb, self);
  g_clear_object (&self->sorter);

  G_OBJECT_CLASS (gtk_invertible_sorter_parent_class)->dispose (object);
}

static void
gtk_invertible_sorter_class_init (GtkInvertibleSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_invertible_sorter_compare;

  object_class->get_property = gtk_invertible_sorter_get_property;
  object_class->set_property = gtk_invertible_sorter_set_property;
  object_class->dispose = gtk_invertible_sorter_dispose;

  /**
   * GtkInvertibleSorter:sorter:
   *
   * The underlying sorter
   */
  properties[PROP_SORTER] =
      g_param_spec_object ("sorter",
                          P_("Sorter"),
                          P_("The underlying sorter"),
                          GTK_TYPE_SORTER,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkInvertibleSorter:direction:
   *
   * The direction to sort in
   */
  properties[PROP_DIRECTION] =
      g_param_spec_enum ("direction",
                         P_("Direction"),
                         P_("The direction to sort in"),
                         GTK_TYPE_SORT_TYPE,
                         GTK_SORT_ASCENDING,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
gtk_invertible_sorter_init (GtkInvertibleSorter *self)
{
  self->direction = GTK_SORT_ASCENDING;
}

GtkSorter *
gtk_invertible_sorter_new (GtkSorter *sorter)
{
  return g_object_new (GTK_TYPE_INVERTIBLE_SORTER,
                       "sorter", sorter,
                       NULL);
}

GtkSorter *
gtk_invertible_sorter_get_sorter (GtkInvertibleSorter *self)
{
  g_return_val_if_fail (GTK_IS_INVERTIBLE_SORTER (self), NULL);

  return self->sorter;
}

static void
changed_cb (GtkSorter *sorter, int change, gpointer data)
{
  gtk_sorter_changed (GTK_SORTER (data), change);
}

void
gtk_invertible_sorter_set_sorter (GtkInvertibleSorter *self,
                                  GtkSorter           *sorter)
{
  g_return_if_fail (GTK_IS_INVERTIBLE_SORTER (self));
  g_return_if_fail (sorter == NULL || GTK_IS_SORTER (sorter));

  if (self->sorter == sorter)
    return;

  g_signal_handlers_disconnect_by_func (self->sorter, changed_cb, self);
  g_set_object (&self->sorter, sorter);
  if (self->sorter)
    g_signal_connect (self->sorter, "changed", G_CALLBACK (changed_cb), self);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORTER]);
}

GtkSortType
gtk_invertible_sorter_get_direction (GtkInvertibleSorter *self)
{
  g_return_val_if_fail (GTK_IS_INVERTIBLE_SORTER (self), GTK_SORT_ASCENDING);

  return self->direction;
}

void
gtk_invertible_sorter_set_direction (GtkInvertibleSorter *self,
                                     GtkSortType          direction)
{
  g_return_if_fail (GTK_IS_INVERTIBLE_SORTER (self));

  if (self->direction == direction)
    return;

  self->direction = direction;

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_INVERTED);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DIRECTION]);
}

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

#include "gtkcustomsorter.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

struct _GtkCustomSorter
{
  GtkSorter parent_instance;

  GCompareDataFunc sort_func;
  gpointer user_data;
  GDestroyNotify user_destroy;
};

G_DEFINE_TYPE (GtkCustomSorter, gtk_custom_sorter, GTK_TYPE_SORTER)

static int
gtk_custom_sorter_compare (GtkSorter *sorter,
                           gpointer   item1,
                           gpointer   item2)
{
  GtkCustomSorter *self = GTK_CUSTOM_SORTER (sorter);

  return self->sort_func (item1, item2, self->user_data);
}

static void
gtk_custom_sorter_dispose (GObject *object)
{
  GtkCustomSorter *self = GTK_CUSTOM_SORTER (object);

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  G_OBJECT_CLASS (gtk_custom_sorter_parent_class)->dispose (object);
}

static void
gtk_custom_sorter_class_init (GtkCustomSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_custom_sorter_compare;

  object_class->dispose = gtk_custom_sorter_dispose;
}

static void
gtk_custom_sorter_init (GtkCustomSorter *self)
{
  gtk_sorter_changed (GTK_SORTER (self));
}

GtkSorter *
gtk_custom_sorter_new (GCompareDataFunc sort_func,
                       gpointer         user_data,
                       GDestroyNotify   user_destroy)
{
  GtkCustomSorter *sorter;

  sorter = g_object_new (GTK_TYPE_CUSTOM_SORTER, NULL);

  sorter->sort_func = sort_func;
  sorter->user_data = user_data;
  sorter->user_destroy = user_destroy;

  return GTK_SORTER (sorter);
}

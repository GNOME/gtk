/*
 * Copyright © 2019 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkfilters.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

struct _GtkCustomFilter
{
  GtkFilter parent_instance;

  GtkCustomFilterFunc filter_func;
  gpointer user_data;
  GDestroyNotify user_destroy;
};

G_DEFINE_TYPE (GtkCustomFilter, gtk_custom_filter, GTK_TYPE_FILTER)

static gboolean
gtk_custom_filter_filter (GtkFilter *filter,
                          gpointer   item)
{
  GtkCustomFilter *self = GTK_CUSTOM_FILTER (filter);

  return self->filter_func (item, self->user_data);
}

static void
gtk_custom_filter_dispose (GObject *object)
{
  GtkCustomFilter *self = GTK_CUSTOM_FILTER (object);

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  G_OBJECT_CLASS (gtk_custom_filter_parent_class)->dispose (object);
}

static void
gtk_custom_filter_class_init (GtkCustomFilterClass *class)
{
  GtkFilterClass *filter_class = GTK_FILTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  filter_class->filter = gtk_custom_filter_filter;

  object_class->dispose = gtk_custom_filter_dispose;
}

static void
gtk_custom_filter_init (GtkCustomFilter *self)
{
}

/**
 * gtk_custom_filter_new:
 * @filter_func: 
 * @user_data: (allow none): user data to pass to @filter_func
 * @user_destroy: destory notify 
 *
 * Creates a new filter using the given @filter_func to filter
 * items.
 *
 * If the filter func changes its filtering behavior,
 * gtk_filter_changed() needs to be called.
 *
 * Returns: a new #GtkFilter
 **/
GtkFilter *
gtk_custom_filter_new (GtkCustomFilterFunc     filter_func,
                       gpointer                user_data,
                       GDestroyNotify          user_destroy)
{
  GtkCustomFilter *result;

  result = g_object_new (GTK_TYPE_CUSTOM_FILTER, NULL);

  result->filter_func = filter_func;
  result->user_data = user_data;
  result->user_destroy = user_destroy;

  return GTK_FILTER (result);
}


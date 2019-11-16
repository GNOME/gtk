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

/*** CUSTOM FILTER ***/

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

/*** ANY FILTER ***/

struct _GtkAnyFilter
{
  GtkFilter parent_instance;

  GSequence *filters;
};

static GType
gtk_any_filter_get_item_type (GListModel *list)
{
  return GTK_TYPE_FILTER;
}

static guint
gtk_any_filter_get_n_items (GListModel *list)
{
  GtkAnyFilter *self = GTK_ANY_FILTER (list);

  return g_sequence_get_length (self->filters);
}

static gpointer
gtk_any_filter_get_item (GListModel *list,
                         guint       position)
{
  GtkAnyFilter *self = GTK_ANY_FILTER (list);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->filters, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;
  else
    return g_object_ref (g_sequence_get (iter));
}

static void
gtk_any_filter_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_any_filter_get_item_type;
  iface->get_n_items = gtk_any_filter_get_n_items;
  iface->get_item = gtk_any_filter_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkAnyFilter, gtk_any_filter, GTK_TYPE_FILTER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_any_filter_list_model_init))

static gboolean
gtk_any_filter_filter (GtkFilter *filter,
                       gpointer   item)
{
  GtkAnyFilter *self = GTK_ANY_FILTER (filter);
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (self->filters);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      GtkFilter *child = g_sequence_get (iter);

      if (gtk_filter_filter (child, item))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_any_filter_dispose (GObject *object)
{
  GtkAnyFilter *self = GTK_ANY_FILTER (object);

  g_clear_pointer (&self->filters, g_sequence_free);

  G_OBJECT_CLASS (gtk_any_filter_parent_class)->dispose (object);
}

static void
gtk_any_filter_class_init (GtkAnyFilterClass *class)
{
  GtkFilterClass *filter_class = GTK_FILTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  filter_class->filter = gtk_any_filter_filter;

  object_class->dispose = gtk_any_filter_dispose;
}

static void
gtk_any_filter_init (GtkAnyFilter *self)
{
  self->filters = g_sequence_new (g_object_unref);

  gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_MATCH_NONE);
}

/**
 * gtk_any_filter_new:
 *
 * Creates a new "any" filter.
 *
 * This filter matches an item if any of the filters added to it
 * matches the item.
 * In particular, this means that if no filter has been added to
 * it, the filter matches no item.
 *
 * Returns: a new #GtkFilter
 **/
GtkFilter *
gtk_any_filter_new (void)
{
  return g_object_new (GTK_TYPE_ANY_FILTER, NULL);
}

/**
 * gtk_any_filter_append:
 * @self: a #GtkAnyFilter
 * @filter: (tranfer none): A new filter to use
 *
 * Adds a @filter to @self to use for matching.
 **/
void
gtk_any_filter_append (GtkAnyFilter *self,
                       GtkFilter    *filter)
{
  g_return_if_fail (GTK_IS_ANY_FILTER (self));
  g_return_if_fail (GTK_IS_FILTER (filter));

  g_sequence_append (self->filters, g_object_ref (filter));

  gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_LESS_STRICT);
}

/**
 * gtk_any_filter_remove:
 * @self: a #GtkAnyFilter
 * @position: position of filter to remove
 *
 * Removes the filter at the given @position from the list of filters used
 * by @self.
 * If @position is larger than the number of filters, nothing happens and
 * the function returns.
 **/
void
gtk_any_filter_remove (GtkAnyFilter *self,
                       guint         position)
{
  GSequenceIter *iter;
  guint length;

  length = g_sequence_get_length (self->filters);
  if (position >= length)
    return;

  iter = g_sequence_get_iter_at_pos (self->filters, position);
  g_sequence_remove (iter);

  gtk_filter_changed (GTK_FILTER (self), length == 1 
                                         ? GTK_FILTER_CHANGE_MATCH_NONE
                                         : GTK_FILTER_CHANGE_MORE_STRICT);
}

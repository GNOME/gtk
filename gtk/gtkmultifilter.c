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

#include "gtkmultifilter.h"

#include "gtkfilterprivate.h"
#include "gtkbuildable.h"
#include "gtktypebuiltins.h"

#define GDK_ARRAY_TYPE_NAME GtkFilters
#define GDK_ARRAY_NAME gtk_filters
#define GDK_ARRAY_ELEMENT_TYPE GtkFilter *
#define GDK_ARRAY_FREE_FUNC g_object_unref

#include "gdk/gdkarrayimpl.c"

/*** MULTI FILTER ***/

/**
 * GtkMultiFilter:
 *
 * Base class for filters that combine multiple filters.
 */

/**
 * GtkAnyFilter:
 *
 * Matches an item when at least one of its filters matches.
 *
 * To add filters to a `GtkAnyFilter`, use [method@Gtk.MultiFilter.append].
 */

/**
 * GtkEveryFilter:
 *
 * Matches an item when each of its filters matches.
 *
 * To add filters to a `GtkEveryFilter`, use [method@Gtk.MultiFilter.append].
 */

struct _GtkMultiFilter
{
  GtkFilter parent_instance;

  GtkFilters filters;
};

struct _GtkMultiFilterClass
{
  GtkFilterClass parent_class;

  GtkFilterChange addition_change;
  GtkFilterChange removal_change;
};

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_N_ITEMS,

  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_multi_filter_get_item_type (GListModel *list)
{
  return GTK_TYPE_FILTER;
}

static guint
gtk_multi_filter_get_n_items (GListModel *list)
{
  GtkMultiFilter *self = GTK_MULTI_FILTER (list);

  return gtk_filters_get_size (&self->filters);
}

static gpointer
gtk_multi_filter_get_item (GListModel *list,
                           guint       position)
{
  GtkMultiFilter *self = GTK_MULTI_FILTER (list);

  if (position < gtk_filters_get_size (&self->filters))
    return g_object_ref (gtk_filters_get (&self->filters, position));
  else
    return NULL;
}

static void
gtk_multi_filter_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_multi_filter_get_item_type;
  iface->get_n_items = gtk_multi_filter_get_n_items;
  iface->get_item = gtk_multi_filter_get_item;
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_multi_filter_buildable_add_child (GtkBuildable *buildable,
                                      GtkBuilder   *builder,
                                      GObject      *child,
                                      const char   *type)
{
  if (GTK_IS_FILTER (child))
    gtk_multi_filter_append (GTK_MULTI_FILTER (buildable), g_object_ref (GTK_FILTER (child)));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_multi_filter_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_multi_filter_buildable_add_child;
}

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkMultiFilter, gtk_multi_filter, GTK_TYPE_FILTER,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_multi_filter_list_model_init)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_multi_filter_buildable_init))

static void
gtk_multi_filter_changed_cb (GtkFilter       *filter,
                             GtkFilterChange  change,
                             GtkMultiFilter  *self)
{
  gtk_filter_changed (GTK_FILTER (self), change);
}

static void
gtk_multi_filter_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkMultiFilter *self = GTK_MULTI_FILTER (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, GTK_TYPE_FILTER);
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, gtk_filters_get_size (&self->filters));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_multi_filter_dispose (GObject *object)
{
  GtkMultiFilter *self = GTK_MULTI_FILTER (object);
  guint i;

  for (i = 0; i < gtk_filters_get_size (&self->filters); i++)
    {
      GtkFilter *filter = gtk_filters_get (&self->filters, i);
      g_signal_handlers_disconnect_by_func (filter, gtk_multi_filter_changed_cb, self);
    }

  gtk_filters_clear (&self->filters);

  G_OBJECT_CLASS (gtk_multi_filter_parent_class)->dispose (object);
}

typedef struct _MultiFilterWatchData {
  GHashTable *filter_to_watch;
  GtkFilterWatchCallback callback;

  gpointer user_data;
  GDestroyNotify destroy;
} MultiFilterWatchData;

static void
multi_filter_watch_cb (gpointer item,
                       gpointer user_data)
{
  MultiFilterWatchData *data = (MultiFilterWatchData *) user_data;
  data->callback (item, data->user_data);
}

static gpointer
gtk_multi_filter_watch (GtkFilter              *filter,
                        gpointer                item,
                        GtkFilterWatchCallback  callback,
                        gpointer                user_data,
                        GDestroyNotify          destroy)
{
  MultiFilterWatchData *data;
  GtkMultiFilter *self;

  self = GTK_MULTI_FILTER (filter);

  data = g_new0 (MultiFilterWatchData, 1);
  data->callback = callback;
  data->user_data = user_data;
  data->destroy = destroy;

  data->filter_to_watch = g_hash_table_new (g_direct_hash, g_direct_equal);
  for (size_t i = 0; i < gtk_filters_get_size (&self->filters); i++)
    {
      GtkFilter *child = gtk_filters_get (&self->filters, i);

      g_hash_table_insert (data->filter_to_watch,
                           child,
                           gtk_filter_watch (child, item,
                                             multi_filter_watch_cb,
                                             data,
                                             NULL));
    }

  return g_steal_pointer (&data);
}

static void
gtk_multi_filter_unwatch (GtkFilter *filter,
                          gpointer   watch)
{
  MultiFilterWatchData *data = (MultiFilterWatchData *) watch;
  GHashTableIter iter;
  gpointer child_filter;
  gpointer child_watch;

  g_assert (data->filter_to_watch != NULL);

  g_hash_table_iter_init (&iter, data->filter_to_watch);
  while (g_hash_table_iter_next (&iter, &child_filter, &child_watch) && child_watch)
    gtk_filter_unwatch (child_filter, child_watch);

  g_clear_pointer (&data->filter_to_watch, g_hash_table_destroy);
  g_free (data);
}

static void
gtk_multi_filter_class_init (GtkMultiFilterClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkFilterClassPrivate *filter_class_priv = G_TYPE_CLASS_GET_PRIVATE (class, GTK_TYPE_FILTER, GtkFilterClassPrivate);

  object_class->get_property = gtk_multi_filter_get_property;
  object_class->dispose = gtk_multi_filter_dispose;

  filter_class_priv->watch = gtk_multi_filter_watch;
  filter_class_priv->unwatch = gtk_multi_filter_unwatch;

  /**
   * GtkMultiFilter:item-type:
   *
   * The type of items.
   *
   * See [method@Gio.ListModel.get_item_type].
   *
   * Since: 4.8
   */
  properties[PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        GTK_TYPE_FILTER,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMultiFilter:n-items:
   *
   * The number of items.
   *
   * See [method@Gio.ListModel.get_n_items].
   *
   * Since: 4.8
   */
  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_multi_filter_init (GtkMultiFilter *self)
{
  gtk_filters_init (&self->filters);
}

/**
 * gtk_multi_filter_append:
 * @self: a multi filter
 * @filter: (transfer full): a filter to add
 *
 * Adds a filter.
 */
void
gtk_multi_filter_append (GtkMultiFilter *self,
                         GtkFilter    *filter)
{
  g_return_if_fail (GTK_IS_MULTI_FILTER (self));
  g_return_if_fail (GTK_IS_FILTER (filter));

  g_signal_connect (filter, "changed", G_CALLBACK (gtk_multi_filter_changed_cb), self);
  gtk_filters_append (&self->filters, filter);
  g_list_model_items_changed (G_LIST_MODEL (self), gtk_filters_get_size (&self->filters) - 1, 0, 1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);

  gtk_filter_changed (GTK_FILTER (self),
                      GTK_MULTI_FILTER_GET_CLASS (self)->addition_change);
}

/**
 * gtk_multi_filter_remove:
 * @self: a multi filter
 * @position: position of filter to remove
 *
 * Removes a filter.
 *
 * If @position is larger than the number of filters,
 * nothing happens.
 **/
void
gtk_multi_filter_remove (GtkMultiFilter *self,
                         guint           position)
{
  guint length;
  GtkFilter *filter;

  length = gtk_filters_get_size (&self->filters);
  if (position >= length)
    return;

  filter = gtk_filters_get (&self->filters, position);
  g_signal_handlers_disconnect_by_func (filter, gtk_multi_filter_changed_cb, self);
  gtk_filters_splice (&self->filters, position, 1, FALSE, NULL, 0);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);

  gtk_filter_changed (GTK_FILTER (self),
                      GTK_MULTI_FILTER_GET_CLASS (self)->removal_change);
}

/*** ANY FILTER ***/

struct _GtkAnyFilter
{
  GtkMultiFilter parent_instance;
};

struct _GtkAnyFilterClass
{
  GtkMultiFilterClass parent_class;
};

G_DEFINE_TYPE (GtkAnyFilter, gtk_any_filter, GTK_TYPE_MULTI_FILTER)

static gboolean
gtk_any_filter_match (GtkFilter *filter,
                      gpointer   item)
{
  GtkMultiFilter *self = GTK_MULTI_FILTER (filter);
  guint i;

  for (i = 0; i < gtk_filters_get_size (&self->filters); i++)
    {
      GtkFilter *child = gtk_filters_get (&self->filters, i);

      if (gtk_filter_match (child, item))
        return TRUE;
    }

  return FALSE;
}

static GtkFilterMatch
gtk_any_filter_get_strictness (GtkFilter *filter)
{
  GtkMultiFilter *self = GTK_MULTI_FILTER (filter);
  guint i;
  GtkFilterMatch result = GTK_FILTER_MATCH_NONE;

  for (i = 0; i < gtk_filters_get_size (&self->filters); i++)
    {
      GtkFilter *child = gtk_filters_get (&self->filters, i);

      switch (gtk_filter_get_strictness (child))
      {
        case GTK_FILTER_MATCH_SOME:
          result = GTK_FILTER_MATCH_SOME;
          break;
        case GTK_FILTER_MATCH_NONE:
          break;
        case GTK_FILTER_MATCH_ALL:
          return GTK_FILTER_MATCH_ALL;
        default:
          g_return_val_if_reached (GTK_FILTER_MATCH_NONE);
          break;
      }
    }

  return result;
}

static void
gtk_any_filter_class_init (GtkAnyFilterClass *class)
{
  GtkMultiFilterClass *multi_filter_class = GTK_MULTI_FILTER_CLASS (class);
  GtkFilterClass *filter_class = GTK_FILTER_CLASS (class);

  multi_filter_class->addition_change = GTK_FILTER_CHANGE_LESS_STRICT_REWATCH;
  multi_filter_class->removal_change = GTK_FILTER_CHANGE_MORE_STRICT_REWATCH;

  filter_class->match = gtk_any_filter_match;
  filter_class->get_strictness = gtk_any_filter_get_strictness;
}

static void
gtk_any_filter_init (GtkAnyFilter *self)
{
}

/**
 * gtk_any_filter_new:
 *
 * Creates a new empty "any" filter.
 *
 * Use [method@Gtk.MultiFilter.append] to add filters to it.
 *
 * This filter matches an item if any of the filters added to it
 * matches the item. In particular, this means that if no filter
 * has been added to it, the filter matches no item.
 *
 * Returns: a new `GtkAnyFilter`
 */
GtkAnyFilter *
gtk_any_filter_new (void)
{
  return g_object_new (GTK_TYPE_ANY_FILTER, NULL);
}

/*** EVERY FILTER ***/

struct _GtkEveryFilter
{
  GtkMultiFilter parent_instance;
};

struct _GtkEveryFilterClass
{
  GtkMultiFilterClass parent_class;
};

G_DEFINE_TYPE (GtkEveryFilter, gtk_every_filter, GTK_TYPE_MULTI_FILTER)

static gboolean
gtk_every_filter_match (GtkFilter *filter,
                        gpointer   item)
{
  GtkMultiFilter *self = GTK_MULTI_FILTER (filter);
  guint i;

  for (i = 0; i < gtk_filters_get_size (&self->filters); i++)
    {
      GtkFilter *child = gtk_filters_get (&self->filters, i);

      if (!gtk_filter_match (child, item))
        return FALSE;
    }

  return TRUE;
}

static GtkFilterMatch
gtk_every_filter_get_strictness (GtkFilter *filter)
{
  GtkMultiFilter *self = GTK_MULTI_FILTER (filter);
  guint i;
  GtkFilterMatch result = GTK_FILTER_MATCH_ALL;

  for (i = 0; i < gtk_filters_get_size (&self->filters); i++)
    {
      GtkFilter *child = gtk_filters_get (&self->filters, i);

      switch (gtk_filter_get_strictness (child))
      {
        case GTK_FILTER_MATCH_SOME:
          result = GTK_FILTER_MATCH_SOME;
          break;
        case GTK_FILTER_MATCH_NONE:
          return GTK_FILTER_MATCH_NONE;
        case GTK_FILTER_MATCH_ALL:
          break;
        default:
          g_return_val_if_reached (GTK_FILTER_MATCH_NONE);
          break;
      }
    }

  return result;
}

static void
gtk_every_filter_class_init (GtkEveryFilterClass *class)
{
  GtkMultiFilterClass *multi_filter_class = GTK_MULTI_FILTER_CLASS (class);
  GtkFilterClass *filter_class = GTK_FILTER_CLASS (class);

  multi_filter_class->addition_change = GTK_FILTER_CHANGE_MORE_STRICT_REWATCH;
  multi_filter_class->removal_change = GTK_FILTER_CHANGE_LESS_STRICT_REWATCH;

  filter_class->match = gtk_every_filter_match;
  filter_class->get_strictness = gtk_every_filter_get_strictness;
}

static void
gtk_every_filter_init (GtkEveryFilter *self)
{
}

/**
 * gtk_every_filter_new:
 *
 * Creates a new empty "every" filter.
 *
 * Use [method@Gtk.MultiFilter.append] to add filters to it.
 *
 * This filter matches an item if each of the filters added to it
 * matches the item. In particular, this means that if no filter
 * has been added to it, the filter matches every item.
 *
 * Returns: a new `GtkEveryFilter`
 */
GtkEveryFilter *
gtk_every_filter_new (void)
{
  return g_object_new (GTK_TYPE_EVERY_FILTER, NULL);
}


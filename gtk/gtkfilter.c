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

#include "gtkfilterprivate.h"

#include "gtktypebuiltins.h"
#include "gtkprivate.h"

/**
 * GtkFilter:
 *
 * Describes the filtering to be performed by a [class@Gtk.FilterListModel].
 *
 * The model will use the filter to determine if it should include items
 * or not by calling [method@Gtk.Filter.match] for each item and only
 * keeping the ones that the function returns true for.
 *
 * Filters may change what items they match through their lifetime. In that
 * case, they will emit the [signal@Gtk.Filter::changed] signal to notify
 * that previous filter results are no longer valid and that items should
 * be checked again via [method@Gtk.Filter.match].
 *
 * GTK provides various pre-made filter implementations for common filtering
 * operations. These filters often include properties that can be linked to
 * various widgets to easily allow searches.
 *
 * However, in particular for large lists or complex search methods, it is
 * also possible to subclass `GtkFilter` and provide one's own filter.
 */

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_CODE (GtkFilter, gtk_filter, G_TYPE_OBJECT,
                         g_type_add_class_private (g_define_type_id, sizeof (GtkFilterClassPrivate)))

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _ExpressionWatchData {
  GtkExpression *expression;
  gpointer item;

  GtkExpressionWatch *watch;
  GtkFilterWatchCallback callback;

  gpointer user_data;
  GDestroyNotify destroy;
} ExpressionWatchData;

static void
expression_watch_cb (gpointer user_data)
{
  ExpressionWatchData *data = (ExpressionWatchData *) user_data;
  data->callback (data->item, data->user_data);
}

static gpointer
gtk_filter_default_watch (GtkFilter              *self,
                          gpointer                item,
                          GtkFilterWatchCallback  callback,
                          gpointer                user_data,
                          GDestroyNotify          destroy)
{
  GParamSpec *pspec;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (self), "expression");
  if (pspec && g_type_is_a (pspec->value_type, GTK_TYPE_EXPRESSION))
    {
      ExpressionWatchData *data;
      GtkExpression *expression;

      g_object_get (self, "expression", &expression, NULL);

      data = g_new0 (ExpressionWatchData, 1);
      data->item = item;
      data->callback = callback;
      data->user_data = user_data;
      data->destroy = destroy;
      data->watch = gtk_expression_watch (expression,
                                          item,
                                          expression_watch_cb,
                                          data,
                                          NULL);

      gtk_expression_unref (expression);

      return g_steal_pointer (&data);
    }

  return NULL;
}

static void
gtk_filter_default_unwatch (GtkFilter *filter,
                            gpointer   watch)
{
  ExpressionWatchData *data = (ExpressionWatchData *) watch;
  g_clear_pointer (&data->watch, gtk_expression_watch_unwatch);
  g_free (data);
}

static gboolean
gtk_filter_default_match (GtkFilter *self,
                          gpointer   item)
{
  g_critical ("Filter of type '%s' does not implement GtkFilter::match", G_OBJECT_TYPE_NAME (self));

  return FALSE;
}

static GtkFilterMatch
gtk_filter_default_get_strictness (GtkFilter *self)
{
  return GTK_FILTER_MATCH_SOME;
}

static void
gtk_filter_class_init (GtkFilterClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkFilterClassPrivate *filter_private_class = G_TYPE_CLASS_GET_PRIVATE (class, GTK_TYPE_FILTER, GtkFilterClassPrivate);

  class->match = gtk_filter_default_match;
  class->get_strictness = gtk_filter_default_get_strictness;

  filter_private_class->watch = gtk_filter_default_watch;
  filter_private_class->unwatch = gtk_filter_default_unwatch;

  /**
   * GtkFilter::changed:
   * @self: the filter
   * @change: how the filter changed
   *
   * Emitted whenever the filter changed.
   *
   * Users of the filter should then check items again via
   * [method@Gtk.Filter.match].
   *
   * `GtkFilterListModel` handles this signal automatically.
   *
   * Depending on the @change parameter, not all items need
   * to be checked, but only some. Refer to the [enum@Gtk.FilterChange]
   * documentation for details.
   */
  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_FILTER_CHANGE);
  g_signal_set_va_marshaller (signals[CHANGED],
                              G_TYPE_FROM_CLASS (gobject_class),
                              g_cclosure_marshal_VOID__ENUMv);
}

static void
gtk_filter_init (GtkFilter *self)
{
}

/**
 * gtk_filter_match:
 * @self: a filter
 * @item: (type GObject) (transfer none): The item to check
 *
 * Checks if the given @item is matched by the filter or not.
 *
 * Returns: true if the filter matches the item
 */
gboolean
gtk_filter_match (GtkFilter *self,
                  gpointer   item)
{
  g_return_val_if_fail (GTK_IS_FILTER (self), FALSE);
  g_return_val_if_fail (item != NULL, FALSE);

  return GTK_FILTER_GET_CLASS (self)->match (self, item);
}

/**
 * gtk_filter_get_strictness:
 * @self: a filter
 *
 * Gets the known strictness of a filter.
 *
 * If the strictness is not known, [enum@Gtk.FilterMatch.some] is returned.
 *
 * This value may change after emission of the [signal@Gtk.Filter::changed]
 * signal.
 *
 * This function is meant purely for optimization purposes. Filters can
 * choose to omit implementing it, but `GtkFilterListModel` uses it.
 *
 * Returns: the strictness of @self
 */
GtkFilterMatch
gtk_filter_get_strictness (GtkFilter *self)
{
  g_return_val_if_fail (GTK_IS_FILTER (self), GTK_FILTER_MATCH_SOME);

  return GTK_FILTER_GET_CLASS (self)->get_strictness (self);
}

/**
 * gtk_filter_changed:
 * @self: a filter
 * @change: how the filter changed
 *
 * Notifies all users of the filter that it has changed.
 *
 * This emits the [signal@Gtk.Filter::changed] signal. Users
 * of the filter should then check items again via
 * [method@Gtk.Filter.match].
 *
 * Depending on the @change parameter, not all items need to
 * be changed, but only some. Refer to the [enum@Gtk.FilterChange]
 * documentation for details.
 *
 * This function is intended for implementers of `GtkFilter`
 * subclasses and should not be called from other functions.
 */
void
gtk_filter_changed (GtkFilter       *self,
                    GtkFilterChange  change)
{
  g_return_if_fail (GTK_IS_FILTER (self));

  g_signal_emit (self, signals[CHANGED], 0, change);
}

/*<private>
 * gtk_filter_watch:
 * @self: a filter
 * @item: (type GObject) (transfer none): The item to watch
 *
 * Watches the the given @item for property changes.
 *
 * Callers are responsible to keep this watch as long as both
 * @self and @item are alive. To destroy the watch, use
 * gtk_filter_unwatch.
 *
 * Returns: (transfer full) (nullable): the expression watch
 *
 * Since: 4.20
 */
gpointer
gtk_filter_watch (GtkFilter              *self,
                  gpointer                item,
                  GtkFilterWatchCallback  callback,
                  gpointer                user_data,
                  GDestroyNotify          destroy)
{
  GtkFilterClassPrivate *priv;
  GtkFilterClass *class;

  g_return_val_if_fail (GTK_IS_FILTER (self), NULL);

  class = GTK_FILTER_GET_CLASS (self);
  priv = G_TYPE_CLASS_GET_PRIVATE (class, GTK_TYPE_FILTER, GtkFilterClassPrivate);

  return priv->watch (self, item, callback, user_data, destroy);
}

/*<private>
 * gtk_filter_unwatch:
 * @self: a filter
 * @watch: (transfer full): The item to watch
 *
 * Stops @watch. This is only called with what was previously returned
 * by [vfunc@Gtk.Filter.watch].
 *
 * Since: 4.20
 */
void
gtk_filter_unwatch (GtkFilter *self,
                    gpointer   watch)
{
  GtkFilterClassPrivate *priv;
  GtkFilterClass *class;

  g_return_if_fail (GTK_IS_FILTER (self));
  g_return_if_fail (watch != NULL);

  class = GTK_FILTER_GET_CLASS (self);
  priv = G_TYPE_CLASS_GET_PRIVATE (class, GTK_TYPE_FILTER, GtkFilterClassPrivate);

  priv->unwatch (self, watch);
}

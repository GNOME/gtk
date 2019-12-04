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

#include "gtkfilter.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

/**
 * SECTION:gtkfilter
 * @Title: GtkFilter
 * @Short_description: Filtering items in GTK
 * @See_also: #GtkFilerListModel
 *
 * #GtkFilter is the way to describe filters to be used in #GtkFilterListModel.
 * 
 * The model will use a filter to determine if it should filter items or not
 * by calling gtk_filter_filter() for each item and only keeping the ones
 * visible that the function returns %TRUE for.
 *
 * Filters may change what items they match through their lifetime. In that
 * case, they can call gtk_filter_changed() which will emit the #GtkFilter:changed
 * signal to notify that previous filter results are no longer valid and that
 * items should be checked via gtk_filter_filter() again.
 *
 * GTK provides various premade filter implementations for common filtering
 * operations. These filters often include properties that can be linked to
 * various widgets to easily allow searches.  
 *
 * However, in particular for large lists or complex search methods, it is
 * also possible to subclass #GtkFilter and provide one's own filter.
 */

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkFilter, gtk_filter, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0 };

static gboolean
gtk_filter_default_filter (GtkFilter *self,
                           gpointer   item)
{
  g_critical ("Filter of type '%s' does not implement GtkFilter::filter", G_OBJECT_TYPE_NAME (self));

  return FALSE;
}

static void
gtk_filter_class_init (GtkFilterClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  class->filter = gtk_filter_default_filter;

  /**
   * GtkFilter:changed:
   * @self: The #GtkFilter
   * @change: how the filter changed
   *
   * This signal is emitted whenever the filter changed. Users of the filter
   * should then check items again via gtk_filter_filter().
   *
   * #GtkFilterListModel handles this signal automatically.
   *
   * Depending on the @change parameter, not all items need to be changed, but
   * only some. Refer to the #GtkFilterChange documentation for details.
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
 * gtk_filter_filter:
 * @self: a #GtkFilter
 * @item: (type GObject) (transfer none): The item to check
 *
 * Checks if the given @item is matched by the filter or not. 
 *
 * Returns: %TRUE if the filter matches the item and a filter model should
 *     keep it, %FALSE if not.
 */
gboolean
gtk_filter_filter (GtkFilter *self,
                   gpointer   item)
{
  g_return_val_if_fail (GTK_IS_FILTER (self), FALSE);
  g_return_val_if_fail (item != NULL, FALSE);

  return GTK_FILTER_GET_CLASS (self)->filter (self, item);
}

/**
 * gtk_filter_changed:
 * @self: a #GtkFilter
 * @change: How the filter changed
 *
 * Emits the #GtkFilter:changed signal to notify all users of the filter that
 * the filter changed. Users of the filter should then check items again via
 * gtk_filter_filter().
 *
 * Depending on the @change parameter, not all items need to be changed, but
 * only some. Refer to the #GtkFilterChange documentation for details.
 *
 * This function is intended for implementors of #GtkFilter subclasses and
 * should not be called from other functions.
 */
void
gtk_filter_changed (GtkFilter       *self,
                    GtkFilterChange  change)
{
  g_return_if_fail (GTK_IS_FILTER (self));

  g_signal_emit (self, signals[CHANGED], 0, change);
}


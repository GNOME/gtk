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

/**
 * SECTION:gtksorter
 * @title: GtkSorter
 * @Short_description: Sorting items
 * @See_also: #GtkSortListModel
 *
 * #GtkSorter is the way to describe sorting criteria.
 * Its primary user is #GtkSortListModel.
 *
 * The model will use a sorter to determine the order in which its items should appear
 * by calling gtk_sorter_compare() for pairs of items.
 *
 * Sorters may change their sorting behavior through their lifetime. In that case,
 * they call gtk_sorter_changed(), which will emit the #GtkSorter::changed signal to
 * notify that the sort order is no longer valid and should be updated by calling
 * gtk_sorter_compare() again.
 *
 * GTK provides various pre-made sorter implementations for common sorting operations.
 * #GtkColumnView has built-in support for sorting lists via the #GtkColumnViewColumn:sorter
 * property, where the user can change the sorting by clicking on list headers.
 *
 * Of course, in particular for large lists, it is also possible to subclass #GtkSorter
 * and provide one's own sorter.
 */

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkSorter, gtk_sorter, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0 };

static GtkOrdering
gtk_sorter_default_compare (GtkSorter *self,
                            gpointer   item1,
                            gpointer   item2)
{
  g_critical ("Sorter of type '%s' does not implement GtkSorter::compare", G_OBJECT_TYPE_NAME (self));

  return GTK_ORDERING_EQUAL;
}

static GtkSorterOrder
gtk_sorter_default_get_order (GtkSorter *self)
{
  return GTK_SORTER_ORDER_PARTIAL;
}

static void
gtk_sorter_class_init (GtkSorterClass *class)
{
  class->compare = gtk_sorter_default_compare;
  class->get_order = gtk_sorter_default_get_order;

  /**
   * GtkSorter::changed:
   * @self: The #GtkSorter
   * @change: how the sorter changed
   *
   * This signal is emitted whenever the sorter changed. Users of the sorter
   * should then update the sort order again via gtk_sorter_compare().
   *
   * #GtkSortListModel handles this signal automatically.
   *
   * Depending on the @change parameter, it may be possible to update
   * the sort order without a full resorting. Refer to the #GtkSorterChange
   * documentation for details.
   */
  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SORTER_CHANGE);
  g_signal_set_va_marshaller (signals[CHANGED],
                              G_TYPE_FROM_CLASS (class),
                              g_cclosure_marshal_VOID__ENUMv);
}

static void
gtk_sorter_init (GtkSorter *self)
{
}

/**
 * gtk_sorter_compare:
 * @self: a #GtkSorter
 * @item1: (type GObject) (transfer none): first item to compare
 * @item2: (type GObject) (transfer none): second item to compare
 *
 * Compares two given items according to the sort order implemented
 * by the sorter.
 *
 * Sorters implement a partial order:
 * * It is reflexive, ie a = a
 * * It is antisymmetric, ie if a < b and b < a, then a = b
 * * It is transitive, ie given any 3 items with a ≤ b and b ≤ c,
 *   then a ≤ c
 * 
 * The sorter  may signal it conforms to additional constraints
 * via the return value of gtk_sorter_get_order().
 *
 * Returns: %GTK_ORDERING_EQUAL if @item1 == @item2,
 *     %GTK_ORDERING_SMALLER if @item1 < @item2,
 *     %GTK_ORDERING_LARGER if @item1 > @item2
 */
GtkOrdering
gtk_sorter_compare (GtkSorter *self,
                    gpointer   item1,
                    gpointer   item2)
{
  GtkOrdering result;

  g_return_val_if_fail (GTK_IS_SORTER (self), GTK_ORDERING_EQUAL);
  g_return_val_if_fail (item1 && item2, GTK_ORDERING_EQUAL);

  if (item1 == item2)
    return GTK_ORDERING_EQUAL;

  result = GTK_SORTER_GET_CLASS (self)->compare (self, item1, item2);

#if G_ENABLE_DEBUG
  if (result < -1 || result > 1)
    {
      g_critical ("A sorter of type \"%s\" returned %d, which is not a valid GtkOrdering result.\n"
                  "Did you forget to call gtk_ordering_from_cmpfunc()?",
                  G_OBJECT_TYPE_NAME (self), (int) result);
    }
#endif

  return result;
}

/**
 * gtk_sorter_get_order:
 * @self: a #GtkSorter
 *
 * Gets the order that @self conforms to. See #GtkSorterOrder for details
 * of the possible return values.
 *
 * This function is intended to allow optimizations.
 *
 * Returns: The order
 **/
GtkSorterOrder
gtk_sorter_get_order (GtkSorter *self)
{
  g_return_val_if_fail (GTK_IS_SORTER (self), GTK_SORTER_ORDER_PARTIAL);

  return GTK_SORTER_GET_CLASS (self)->get_order (self);
}

/**
 * gtk_sorter_changed:
 * @self: a #GtkSorter
 * @change: How the sorter changed
 *
 * Emits the #GtkSorter::changed signal to notify all users of the sorter
 * that it has changed. Users of the sorter should then update the sort
 * order via gtk_sorter_compare().
 *
 * Depending on the @change parameter, it may be possible to update
 * the sort order without a full resorting. Refer to the #GtkSorterChange
 * documentation for details.
 *
 * This function is intended for implementors of #GtkSorter subclasses and
 * should not be called from other functions.
 */
void
gtk_sorter_changed (GtkSorter       *self,
                    GtkSorterChange  change)
{
  g_return_if_fail (GTK_IS_SORTER (self));

  g_signal_emit (self, signals[CHANGED], 0, change);
}

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
 * @Short_description: Sorting items in GTK
 * @See_also: #GtkSortListModel
 *
 * #GtkSorter is the way to describe sorting criteria to be used in #GtkSortListModel.
 *
 * The model will use a sorter to determine the order in which its items should appear
 * by calling gtk_sorter_compare() for pairs of items.
 *
 * Sorters may change through their lifetime. In that case, they call gtk_sorter_changed(),
 * which will emit the #GtkSorter::changed signal to notify that the sort order is no
 * longer valid and should be updated by calling gtk_sortre_compare() again.
 *
 * GTK provides various pre-made sorter implementations for common sorting operations.
 * #GtkColumnView and #GtkColumnViewColumn have built-in support for supporting sorted
 * lists where the user can change the sorting by clicking on list headers.
 *
 * However, in particular for large lists, it is also possible to subclass #GtkSorter
 * and provide one's own sorter.
 */

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkSorter, gtk_sorter, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0 };

static int
gtk_sorter_default_compare (GtkSorter *self,
                            gpointer   item1,
                            gpointer   item2)
{
  g_critical ("Sorter of type '%s' does not implement GtkSorter::compare", G_OBJECT_TYPE_NAME (self));

  return 0;
}

static void
gtk_sorter_class_init (GtkSorterClass *class)
{
  class->compare = gtk_sorter_default_compare;

  /**
   * GtkSearch:changed:
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
 * Returns: zero if @item1 == @item2,
 *     a negative value if @item1 < @item2,
 *     a positive value if @item1 > @item2
 */
int
gtk_sorter_compare (GtkSorter *self,
                    gpointer   item1,
                    gpointer   item2)
{
  g_return_val_if_fail (GTK_IS_SORTER (self), 0);
  g_return_val_if_fail (item1 && item2, 0);

  if (item1 == item2)
    return 0;

  return GTK_SORTER_GET_CLASS (self)->compare (self, item1, item2);
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
  g_signal_emit (self, signals[CHANGED], 0, change);
}

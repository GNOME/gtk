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

G_DEFINE_TYPE_WITH_PRIVATE (GtkSorter, gtk_sorter, G_TYPE_OBJECT)

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
   *
   * This signal is emitted whenever the sorter changed. Users of the sorter
   * should then check items again via gtk_sorter_compare().
   */
  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SORTER_CHANGE);
  g_signal_set_va_marshaller (signals[CHANGED],
                              G_TYPE_FROM_CLASS (class),
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

  return GTK_SORTER_GET_CLASS (self)->compare (self, item1, item2);
}

void
gtk_sorter_changed (GtkSorter       *self,
                    GtkSorterChange  change)
{
  g_signal_emit (self, signals[CHANGED], 0, change);
}

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

#ifndef __GTK_SORTER_H__
#define __GTK_SORTER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_SORTER             (gtk_sorter_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSorter, gtk_sorter, GTK, SORTER, GObject)

struct _GtkSorterClass
{
  GObjectClass parent_class;

  int (* compare)                              (GtkSorter      *self,
                                                gpointer        item1,
                                                gpointer        item2);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
  void (*_gtk_reserved8) (void);
};

GDK_AVAILABLE_IN_ALL
int  gtk_sorter_compare (GtkSorter *self,
                         gpointer   item1,
                         gpointer   item2);


GDK_AVAILABLE_IN_ALL
void gtk_sorter_set_sort_direction (GtkSorter   *self,
                                    GtkSortType  direction);

GDK_AVAILABLE_IN_ALL
GtkSortType gtk_sorter_get_sort_direction (GtkSorter *self);

GDK_AVAILABLE_IN_ALL
void gtk_sorter_changed (GtkSorter *self);

G_END_DECLS

#endif /* __GTK_SORTER_H__ */


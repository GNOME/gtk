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

#ifndef __GTK_COLUMN_VIEW_SORTER_H__
#define __GTK_COLUMN_VIEW_SORTER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtksorter.h>
#include <gtk/gtkcolumnviewcolumn.h>

G_BEGIN_DECLS

void      gtk_column_view_sorter_activate_column (GtkSorter           *self,
                                                  GtkColumnViewColumn *column);
void      gtk_column_view_sorter_remove_column   (GtkSorter           *self,
                                                  GtkColumnViewColumn *column);

void      gtk_column_view_sorter_clear           (GtkSorter           *self);

void      gtk_column_view_sorter_set_column      (GtkSorter           *self,
                                                  GtkColumnViewColumn *column,
                                                  GtkSortType          direction);


G_END_DECLS

#endif /* __GTK_COLUMN_VIEW_SORTER_H__ */


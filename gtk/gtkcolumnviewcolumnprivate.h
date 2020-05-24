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

#ifndef __GTK_COLUMN_VIEW_COLUMN_PRIVATE_H__
#define __GTK_COLUMN_VIEW_COLUMN_PRIVATE_H__

#include "gtk/gtkcolumnviewcolumn.h"

#include "gtk/gtkcolumnviewcellprivate.h"

void                    gtk_column_view_column_set_column_view          (GtkColumnViewColumn    *self,
                                                                         GtkColumnView          *view);

void                    gtk_column_view_column_add_cell                 (GtkColumnViewColumn    *self,
                                                                         GtkColumnViewCell      *cell);
void                    gtk_column_view_column_remove_cell              (GtkColumnViewColumn    *self,
                                                                         GtkColumnViewCell      *cell);
GtkColumnViewCell *     gtk_column_view_column_get_first_cell           (GtkColumnViewColumn    *self);

void                    gtk_column_view_column_queue_resize             (GtkColumnViewColumn    *self);
void                    gtk_column_view_column_measure                  (GtkColumnViewColumn    *self,
                                                                         int                    *minimum,
                                                                         int                    *natural);
void                    gtk_column_view_column_allocate                 (GtkColumnViewColumn    *self,
                                                                         int                     offset,
                                                                         int                     size);
void                    gtk_column_view_column_get_allocation           (GtkColumnViewColumn    *self,
                                                                         int                    *offset,
                                                                         int                    *size);

#endif  /* __GTK_COLUMN_VIEW_COLUMN_PRIVATE_H__ */

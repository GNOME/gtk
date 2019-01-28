/*
 * Copyright © 2018 Benjamin Otte
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

#ifndef __GTK_GRID_VIEW_H__
#define __GTK_GRID_VIEW_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_GRID_VIEW         (gtk_grid_view_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkGridView, gtk_grid_view, GTK, GRID_VIEW, GtkWidget)

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_grid_view_new                               (void);

GDK_AVAILABLE_IN_ALL
GListModel *    gtk_grid_view_get_model                         (GtkGridView            *self);
GDK_AVAILABLE_IN_ALL
void            gtk_grid_view_set_model                         (GtkGridView            *self,
                                                                 GListModel             *model);
GDK_AVAILABLE_IN_ALL
guint           gtk_grid_view_get_min_columns                   (GtkGridView            *self);
GDK_AVAILABLE_IN_ALL
void            gtk_grid_view_set_min_columns                   (GtkGridView            *self,
                                                                 guint                   min_columns);
GDK_AVAILABLE_IN_ALL
guint           gtk_grid_view_get_max_columns                   (GtkGridView            *self);
GDK_AVAILABLE_IN_ALL
void            gtk_grid_view_set_max_columns                   (GtkGridView            *self,
                                                                 guint                   max_columns);


G_END_DECLS

#endif  /* __GTK_GRID_VIEW_H__ */

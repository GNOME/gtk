/*
 * Copyright © 2022 Benjamin Otte
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

#ifndef __GTK_CANVAS_H__
#define __GTK_CANVAS_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_CANVAS         (gtk_canvas_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkCanvas, gtk_canvas, GTK, CANVAS, GtkWidget)

GDK_AVAILABLE_IN_ALL
GtkWidget *             gtk_canvas_new                          (GListModel             *children,
                                                                 GtkListItemFactory     *factory);

GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_set_model                    (GtkCanvas              *self,
                                                                 GListModel             *children);
GDK_AVAILABLE_IN_ALL
GListModel *            gtk_canvas_get_model                    (GtkCanvas              *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_set_factory                  (GtkCanvas              *self,
                                                                 GtkListItemFactory     *factory);
GDK_AVAILABLE_IN_ALL
GtkListItemFactory *    gtk_canvas_get_factory                  (GtkCanvas              *self);

GDK_AVAILABLE_IN_ALL
GtkCanvasItem *         gtk_canvas_lookup_item                  (GtkCanvas              *self,
                                                                 gpointer                item);

GDK_AVAILABLE_IN_ALL
const GtkCanvasSize *   gtk_canvas_get_viewport_size            (GtkCanvas              *self);

G_END_DECLS

#endif  /* __GTK_CANVAS_H__ */

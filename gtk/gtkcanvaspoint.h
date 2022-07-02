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


#ifndef __GTK_CANVAS_POINT_H__
#define __GTK_CANVAS_POINT_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_CANVAS_POINT (gtk_canvas_point_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gtk_canvas_point_get_type               (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkCanvasPoint *        gtk_canvas_point_copy                   (const GtkCanvasPoint   *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_point_free                   (GtkCanvasPoint         *self);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_canvas_point_eval                   (const GtkCanvasPoint   *self,
                                                                 float                  *x,
                                                                 float                  *y) G_GNUC_WARN_UNUSED_RESULT;

GDK_AVAILABLE_IN_ALL
GtkCanvasPoint *        gtk_canvas_point_new                    (float                   x,
                                                                 float                   y);
GDK_AVAILABLE_IN_ALL
GtkCanvasPoint *        gtk_canvas_point_new_from_box           (const GtkCanvasBox     *box,
                                                                 float                   origin_x,
                                                                 float                   origin_y);
GDK_AVAILABLE_IN_ALL
GtkCanvasPoint *        gtk_canvas_point_new_from_item          (GtkCanvasItem          *item,
                                                                 float                   origin_x,
                                                                 float                   origin_y,
                                                                 float                   offset_x,
                                                                 float                   offset_y);

G_END_DECLS

#endif /* __GTK_POINT_H__ */

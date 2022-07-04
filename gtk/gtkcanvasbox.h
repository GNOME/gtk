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


#ifndef __GTK_CANVAS_BOX_H__
#define __GTK_CANVAS_BOX_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GTK_TYPE_CANVAS_BOX (gtk_canvas_box_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gtk_canvas_box_get_type               (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkCanvasBox *          gtk_canvas_box_copy                   (const GtkCanvasBox       *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_box_free                   (GtkCanvasBox             *self);

GDK_AVAILABLE_IN_ALL
const GtkCanvasVector * gtk_canvas_box_get_point              (const GtkCanvasBox       *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
const GtkCanvasVector * gtk_canvas_box_get_size               (const GtkCanvasBox       *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
const GtkCanvasVector * gtk_canvas_box_get_origin             (const GtkCanvasBox       *self) G_GNUC_PURE;

GDK_AVAILABLE_IN_ALL
gboolean                gtk_canvas_box_eval                   (const GtkCanvasBox       *self,
                                                               graphene_rect_t          *rect) G_GNUC_WARN_UNUSED_RESULT;

GDK_AVAILABLE_IN_ALL
GtkCanvasBox *          gtk_canvas_box_new                    (const GtkCanvasVector    *point,
                                                               const GtkCanvasVector    *size,
                                                               float                     origin_x,
                                                               float                     origin_y);
GDK_AVAILABLE_IN_ALL
GtkCanvasBox *          gtk_canvas_box_new_points             (const GtkCanvasVector    *point1,
                                                               const GtkCanvasVector    *point2);

GDK_AVAILABLE_IN_ALL
const GtkCanvasBox *    gtk_canvas_box_get_item_bounds        (GtkCanvasItem            *item);
GDK_AVAILABLE_IN_ALL
const GtkCanvasBox *    gtk_canvas_box_get_item_allocation    (GtkCanvasItem            *item);

G_END_DECLS

#endif /* __GTK_BOX_H__ */

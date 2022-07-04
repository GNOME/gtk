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


#ifndef __GTK_CANVAS_VECTOR_H__
#define __GTK_CANVAS_VECTOR_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GTK_TYPE_CANVAS_VECTOR (gtk_canvas_vector_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gtk_canvas_vector_get_type              (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkCanvasVector *       gtk_canvas_vector_copy                  (const GtkCanvasVector  *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_vector_free                  (GtkCanvasVector        *self);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_canvas_vector_eval                  (const GtkCanvasVector  *self,
                                                                 graphene_vec2_t        *result) G_GNUC_WARN_UNUSED_RESULT;

GDK_AVAILABLE_IN_ALL
GtkCanvasVector *       gtk_canvas_vector_new                   (float                   x,
                                                                 float                   y);
GDK_AVAILABLE_IN_ALL
GtkCanvasVector *       gtk_canvas_vector_new_distance          (const GtkCanvasVector  *from,
                                                                 const GtkCanvasVector  *to);
GDK_AVAILABLE_IN_ALL
GtkCanvasVector *       gtk_canvas_vector_new_from_box          (const GtkCanvasBox     *box,
                                                                 float                   origin_x,
                                                                 float                   origin_y);

typedef enum {
  GTK_CANVAS_ITEM_MEASURE_MIN_FOR_MIN,
  GTK_CANVAS_ITEM_MEASURE_MIN_FOR_NAT,
  GTK_CANVAS_ITEM_MEASURE_NAT_FOR_MIN,
  GTK_CANVAS_ITEM_MEASURE_NAT_FOR_NAT
} GtkCanvasItemMeasure;

GDK_AVAILABLE_IN_ALL
const GtkCanvasVector * gtk_canvas_vector_get_item_measure      (GtkCanvasItem          *item,
                                                                 GtkCanvasItemMeasure    measure);
G_END_DECLS

#endif /* __GTK_VECTOR_H__ */

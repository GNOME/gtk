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


#ifndef __GTK_CANVAS_SIZE_H__
#define __GTK_CANVAS_SIZE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_CANVAS_SIZE (gtk_canvas_size_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gtk_canvas_size_get_type                (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkCanvasSize *         gtk_canvas_size_copy                    (const GtkCanvasSize    *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_size_free                    (GtkCanvasSize          *self);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_canvas_size_eval                    (const GtkCanvasSize    *self,
                                                                 float                  *width,
                                                                 float                  *height) G_GNUC_WARN_UNUSED_RESULT;

GDK_AVAILABLE_IN_ALL
GtkCanvasSize *         gtk_canvas_size_new                     (float                   width,
                                                                 float                   height);
GDK_AVAILABLE_IN_ALL
GtkCanvasSize *         gtk_canvas_size_new_from_box            (const GtkCanvasBox     *box);

typedef enum {
  GTK_CANVAS_ITEM_MEASURE_MIN_FOR_MIN,
  GTK_CANVAS_ITEM_MEASURE_MIN_FOR_NAT,
  GTK_CANVAS_ITEM_MEASURE_NAT_FOR_MIN,
  GTK_CANVAS_ITEM_MEASURE_NAT_FOR_NAT
} GtkCanvasItemMeasurement;

GDK_AVAILABLE_IN_ALL
GtkCanvasSize *         gtk_canvas_size_new_measure_item        (GtkCanvasItem          *item,
                                                                 GtkCanvasItemMeasurement measure);

G_END_DECLS

#endif /* __GTK_SIZE_H__ */

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

#include <gtk/gtklayoutmanager.h>

G_BEGIN_DECLS

struct _GtkPosition {
  float relative;
  float absolute;
};

#define GTK_TYPE_CANVAS_LAYOUT (gtk_canvas_layout_get_type ())
#define GTK_TYPE_CANVAS_LAYOUT_CHILD (gtk_canvas_layout_child_get_type ())

/* GtkCanvasLayout */

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkCanvasLayout, gtk_canvas_layout, GTK, CANVAS_LAYOUT, GtkLayoutManager)

GDK_AVAILABLE_IN_ALL
GtkLayoutManager *      gtk_canvas_layout_new    (void);

/* GtkCanvasLayoutChild */

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkCanvasLayoutChild, gtk_canvas_layout_child, GTK, CANVAS_LAYOUT_CHILD, GtkLayoutChild)

GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_layout_child_set_x           (GtkCanvasLayoutChild   *self,
                                                                 const GtkPosition      *position);
GDK_AVAILABLE_IN_ALL
const GtkPosition *     gtk_canvas_layout_child_get_x           (GtkCanvasLayoutChild   *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_layout_child_set_y           (GtkCanvasLayoutChild   *self,
                                                                 const GtkPosition      *position);
GDK_AVAILABLE_IN_ALL
const GtkPosition *     gtk_canvas_layout_child_get_y           (GtkCanvasLayoutChild   *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_layout_child_set_origin_x    (GtkCanvasLayoutChild   *self,
                                                                 const GtkPosition      *position);
GDK_AVAILABLE_IN_ALL
const GtkPosition *     gtk_canvas_layout_child_get_origin_x    (GtkCanvasLayoutChild   *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_layout_child_set_origin_y    (GtkCanvasLayoutChild   *self,
                                                                 const GtkPosition      *position);
GDK_AVAILABLE_IN_ALL
const GtkPosition *     gtk_canvas_layout_child_get_origin_y    (GtkCanvasLayoutChild   *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_layout_child_set_transform   (GtkCanvasLayoutChild   *self,
                                                                 GskTransform           *transform);
GDK_AVAILABLE_IN_ALL
const GskTransform *    gtk_canvas_layout_child_get_transform   (GtkCanvasLayoutChild   *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_layout_child_set_hpolicy     (GtkCanvasLayoutChild   *self,
                                                                 GtkScrollablePolicy     policy);
GDK_AVAILABLE_IN_ALL
GtkScrollablePolicy     gtk_canvas_layout_child_get_hpolicy     (GtkCanvasLayoutChild   *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_canvas_layout_child_set_vpolicy     (GtkCanvasLayoutChild   *self,
                                                                 GtkScrollablePolicy     policy);
GDK_AVAILABLE_IN_ALL
GtkScrollablePolicy     gtk_canvas_layout_child_get_vpolicy     (GtkCanvasLayoutChild   *self);

G_END_DECLS

#endif  /* __GTK_CANVAS_H__ */

/*
 * Copyright (c) 2016 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GTK_RENDER_NODE_VIEW_H_
#define _GTK_RENDER_NODE_VIEW_H_

#include <gtk/gtkbin.h>

#define GTK_TYPE_RENDER_NODE_VIEW            (gtk_render_node_view_get_type())
#define GTK_RENDER_NODE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_RENDER_NODE_VIEW, GtkRenderNodeView))
#define GTK_RENDER_NODE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_RENDER_NODE_VIEW, GtkRenderNodeViewClass))
#define GTK_IS_RENDER_NODE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_RENDER_NODE_VIEW))
#define GTK_IS_RENDER_NODE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_RENDER_NODE_VIEW))
#define GTK_RENDER_NODE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_RENDER_NODE_VIEW, GtkRenderNodeViewClass))


typedef struct _GtkRenderNodeView
{
  GtkWidget parent;
} GtkRenderNodeView;

typedef struct _GtkRenderNodeViewClass
{
  GtkWidgetClass parent;
} GtkRenderNodeViewClass;

G_BEGIN_DECLS

GType           gtk_render_node_view_get_type           (void);

GtkWidget *     gtk_render_node_view_new                (void);

void            gtk_render_node_view_set_render_node    (GtkRenderNodeView      *view,
                                                         GskRenderNode          *node);
GskRenderNode * gtk_render_node_view_get_render_node    (GtkRenderNodeView      *view);
void            gtk_render_node_view_set_viewport       (GtkRenderNodeView      *view,
                                                         const GdkRectangle     *viewport);
void            gtk_render_node_view_get_viewport       (GtkRenderNodeView      *view,
                                                         GdkRectangle           *viewport);
void            gtk_render_node_view_set_clip_region    (GtkRenderNodeView      *view,
                                                         const cairo_region_t   *clip);
const cairo_region_t*
                gtk_render_node_view_get_clip_region    (GtkRenderNodeView      *view);
void            gtk_render_node_view_set_render_region  (GtkRenderNodeView      *view,
                                                         const cairo_region_t   *region);
const cairo_region_t*
                gtk_render_node_view_get_render_region  (GtkRenderNodeView      *view);


G_END_DECLS

#endif // _GTK_RENDER_NODE_VIEW_H_

// vim: set et sw=2 ts=2:

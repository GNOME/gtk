/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_CSS_NODE_UTILS_PRIVATE_H__
#define __GTK_CSS_NODE_UTILS_PRIVATE_H__

#include <gtk/gtkwidget.h>
#include "gtk/gtkcssnodeprivate.h"

G_BEGIN_DECLS

void                    gtk_css_node_style_changed_for_widget   (GtkCssNode            *node,
                                                                 GtkCssStyle           *old_style,
                                                                 GtkCssStyle           *new_style,
                                                                 GtkWidget             *widget);

typedef void            (* GtkCssNodeSizeFunc)                  (GtkCssNode            *cssnode,
                                                                 GtkOrientation         orientation,
                                                                 gint                   for_size,
                                                                 gint                  *minimum,
                                                                 gint                  *natural,
                                                                 gint                  *minimum_baseline,
                                                                 gint                  *natural_baseline,
                                                                 gpointer               get_content_size_data);
void                    gtk_css_node_get_preferred_size         (GtkCssNode            *cssnode,
                                                                 GtkOrientation         orientation,
                                                                 gint                   for_size,
                                                                 gint                  *minimum,
                                                                 gint                  *natural,
                                                                 gint                  *minimum_baseline,
                                                                 gint                  *natural_baseline,
                                                                 GtkCssNodeSizeFunc     get_content_size_func,
                                                                 gpointer               get_content_size_data);
                                                         
typedef void            (* GtkCssNodeAllocateFunc)              (GtkCssNode            *cssnode,
                                                                 const GtkAllocation   *allocation,
                                                                 int                    baseline,
                                                                 GtkAllocation         *out_clip,
                                                                 gpointer               allocate_data);
void                    gtk_css_node_allocate                   (GtkCssNode            *cssnode,
                                                                 const GtkAllocation   *allocation,
                                                                 int                    baseline,
                                                                 GtkAllocation         *out_clip,
                                                                 GtkCssNodeAllocateFunc allocate_func,
                                                                 gpointer               allocate_data);

typedef gboolean        (* GtkCssNodeDrawFunc)                  (GtkCssNode            *cssnode,
                                                                 cairo_t               *cr,
                                                                 int                    width,
                                                                 int                    height,
                                                                 gpointer               data);
void                    gtk_css_node_draw                       (GtkCssNode            *cssnode,
                                                                 cairo_t               *cr,
                                                                 int                    width,
                                                                 int                    height,
                                                                 GtkCssNodeDrawFunc     draw_contents_func,
                                                                 gpointer               draw_contents_data);

G_END_DECLS

#endif /* __GTK_CSS_NODE_UTILS_PRIVATE_H__ */

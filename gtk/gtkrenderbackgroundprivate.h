/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 * 
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
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

#ifndef __GTK_RENDER_BACKGROUND_PRIVATE_H__
#define __GTK_RENDER_BACKGROUND_PRIVATE_H__

#include <glib-object.h>
#include <cairo.h>

#include "gtkcsstypesprivate.h"
#include "gtksnapshot.h"
#include "gtktypes.h"

G_BEGIN_DECLS

void            gtk_css_style_render_background                 (GtkCssStyle          *style,
                                                                 cairo_t              *cr,
                                                                 gdouble               x,
                                                                 gdouble               y,
                                                                 gdouble               width,
                                                                 gdouble               height,
                                                                 GtkJunctionSides      junction);
gboolean        gtk_css_style_render_background_is_opaque       (GtkCssStyle          *style);
void            gtk_css_style_add_background_render_nodes       (GtkCssStyle      *style,
                                                                 GskRenderer      *renderer,
                                                                 GskRenderNode    *parent_node,
                                                                 graphene_rect_t  *bounds,
                                                                 const char       *name,
                                                                 gdouble           x,
                                                                 gdouble           y,
                                                                 gdouble           width,
                                                                 gdouble           height,
                                                                 GtkJunctionSides  junction);
void            gtk_css_style_snapshot_background               (GtkCssStyle          *style,
                                                                 GtkSnapshot          *snapshot,
                                                                 gdouble               width,
                                                                 gdouble               height,
                                                                 GtkJunctionSides      junction);



G_END_DECLS

#endif /* __GTK_RENDER_BACKGROUND_PRIVATE_H__ */

/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
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

#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>
#include <gsk/gskrendernode.h>

G_BEGIN_DECLS

typedef struct _GskConicGradientNode                    GskConicGradientNode;

#define GSK_TYPE_CONIC_GRADIENT_NODE (gsk_conic_gradient_node_get_type())

GDK_AVAILABLE_IN_ALL
GType                   gsk_conic_gradient_node_get_type            (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GskRenderNode *         gsk_conic_gradient_node_new                 (const graphene_rect_t    *bounds,
                                                                     const graphene_point_t   *center,
                                                                     float                     rotation,
                                                                     const GskColorStop       *color_stops,
                                                                     gsize                     n_color_stops);
GDK_AVAILABLE_IN_ALL
const graphene_point_t * gsk_conic_gradient_node_get_center         (const GskRenderNode      *node) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
float                    gsk_conic_gradient_node_get_rotation       (const GskRenderNode      *node) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_2
float                    gsk_conic_gradient_node_get_angle          (const GskRenderNode      *node) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
gsize                    gsk_conic_gradient_node_get_n_color_stops  (const GskRenderNode      *node) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
const GskColorStop *     gsk_conic_gradient_node_get_color_stops    (const GskRenderNode      *node,
                                                                     gsize                    *n_stops);

G_END_DECLS


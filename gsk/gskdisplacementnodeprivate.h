/* GSK - The GTK Scene Kit
 *
 * Copyright 2025  Benjamin Otte
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

G_BEGIN_DECLS

typedef struct _GskDisplacementNode                GskDisplacementNode;

#define GSK_TYPE_DISPLACEMENT_NODE (gsk_displacement_node_get_type())

GType                   gsk_displacement_node_get_type          (void) G_GNUC_CONST;
GskRenderNode *         gsk_displacement_node_new               (const graphene_rect_t    *bounds,
                                                                 GskRenderNode            *child,
                                                                 GskRenderNode            *displacement,
                                                                 const guint               channels[2],
                                                                 const graphene_size_t    *max,
                                                                 const graphene_size_t    *scale,
                                                                 const graphene_point_t   *offset);
GskRenderNode *         gsk_displacement_node_get_child         (const GskRenderNode      *node) G_GNUC_PURE;
GskRenderNode *         gsk_displacement_node_get_displacement  (const GskRenderNode      *node) G_GNUC_PURE;
const guint *           gsk_displacement_node_get_channels      (const GskRenderNode      *node) G_GNUC_PURE;
const graphene_size_t * gsk_displacement_node_get_max           (const GskRenderNode      *node) G_GNUC_PURE;
const graphene_size_t * gsk_displacement_node_get_scale         (const GskRenderNode      *node) G_GNUC_PURE;
const graphene_point_t *gsk_displacement_node_get_offset        (const GskRenderNode      *node) G_GNUC_PURE;

G_END_DECLS


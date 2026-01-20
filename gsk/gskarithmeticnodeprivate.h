/* GSK - The GTK Scene Kit
 *
 * Copyright 2025 Red Hat, Inc
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

typedef struct _GskArithmeticNode                    GskArithmeticNode;

#define GSK_TYPE_ARITHMETIC_NODE (gsk_arithmetic_node_get_type())

GType                   gsk_arithmetic_node_get_type            (void) G_GNUC_CONST;
GskRenderNode *         gsk_arithmetic_node_new                 (const graphene_rect_t  *bounds,
                                                                 GskRenderNode          *first,
                                                                 GskRenderNode          *second,
                                                                 GdkColorState          *color_state,
                                                                 float                   k1,
                                                                 float                   k2,
                                                                 float                   k3,
                                                                 float                   k4);

GskRenderNode *         gsk_arithmetic_node_get_first_child     (const GskRenderNode    *node) G_GNUC_PURE;
GskRenderNode *         gsk_arithmetic_node_get_second_child    (const GskRenderNode    *node) G_GNUC_PURE;
void                    gsk_arithmetic_node_get_factors         (const GskRenderNode    *node,
                                                                 float                  *k1,
                                                                 float                  *k2,
                                                                 float                  *k3,
                                                                 float                  *k4);
GdkColorState *         gsk_arithmetic_node_get_color_state     (const GskRenderNode    *node) G_GNUC_PURE;

G_END_DECLS


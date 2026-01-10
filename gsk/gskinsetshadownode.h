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

G_BEGIN_DECLS

typedef struct _GskInsetShadowNode                    GskInsetShadowNode;

#define GSK_TYPE_INSET_SHADOW_NODE (gsk_inset_shadow_node_get_type())

GDK_AVAILABLE_IN_ALL
GType                   gsk_inset_shadow_node_get_type          (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GskRenderNode *         gsk_inset_shadow_node_new               (const GskRoundedRect     *outline,
                                                                 const GdkRGBA            *color,
                                                                 float                     dx,
                                                                 float                     dy,
                                                                 float                     spread,
                                                                 float                     blur_radius);
GDK_AVAILABLE_IN_ALL
const GskRoundedRect *  gsk_inset_shadow_node_get_outline       (const GskRenderNode      *node) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
const GdkRGBA *         gsk_inset_shadow_node_get_color         (const GskRenderNode      *node) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
float                   gsk_inset_shadow_node_get_dx            (const GskRenderNode      *node) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
float                   gsk_inset_shadow_node_get_dy            (const GskRenderNode      *node) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
float                   gsk_inset_shadow_node_get_spread        (const GskRenderNode      *node) G_GNUC_PURE;
GDK_AVAILABLE_IN_ALL
float                   gsk_inset_shadow_node_get_blur_radius   (const GskRenderNode      *node) G_GNUC_PURE;

G_END_DECLS


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

typedef struct _GskFillNode                     GskFillNode;

#define GSK_TYPE_FILL_NODE                      (gsk_fill_node_get_type())

GDK_AVAILABLE_IN_4_14
GType                   gsk_fill_node_get_type                  (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_4_14
GskRenderNode *         gsk_fill_node_new                       (GskRenderNode            *child,
                                                                 GskPath                  *path,
                                                                 GskFillRule               fill_rule);
GDK_AVAILABLE_IN_4_14
GskRenderNode *         gsk_fill_node_get_child                 (const GskRenderNode      *node);
GDK_AVAILABLE_IN_4_14
GskPath *               gsk_fill_node_get_path                  (const GskRenderNode      *node);
GDK_AVAILABLE_IN_4_14
GskFillRule             gsk_fill_node_get_fill_rule             (const GskRenderNode      *node);

G_END_DECLS


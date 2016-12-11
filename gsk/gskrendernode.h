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

#ifndef __GSK_RENDER_NODE_H__
#define __GSK_RENDER_NODE_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GSK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_RENDER_NODE (gsk_render_node_get_type ())

#define GSK_IS_RENDER_NODE(obj) ((obj) != NULL)

typedef struct _GskRenderNode           GskRenderNode;

GDK_AVAILABLE_IN_3_90
GType gsk_render_node_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_render_node_ref                     (GskRenderNode *node);
GDK_AVAILABLE_IN_3_90
void                    gsk_render_node_unref                   (GskRenderNode *node);

GDK_AVAILABLE_IN_3_90
GskRenderNodeType       gsk_render_node_get_node_type           (GskRenderNode *node);

GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_texture_node_new                    (GskTexture               *texture,
                                                                 const graphene_rect_t    *bounds);

GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_cairo_node_new                      (const graphene_rect_t    *bounds);
GDK_AVAILABLE_IN_3_90
cairo_t *               gsk_cairo_node_get_draw_context         (GskRenderNode            *node,
                                                                 GskRenderer              *renderer);

GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_container_node_new                  (void);

GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_render_node_get_parent              (GskRenderNode *node);
GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_render_node_get_first_child         (GskRenderNode *node);
GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_render_node_get_last_child          (GskRenderNode *node);
GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_render_node_get_next_sibling        (GskRenderNode *node);
GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_render_node_get_previous_sibling    (GskRenderNode *node);

GDK_AVAILABLE_IN_3_90
GskRenderNode *         gsk_render_node_append_child            (GskRenderNode *node,
                                                                 GskRenderNode *child);
GDK_AVAILABLE_IN_3_90
guint                   gsk_render_node_get_n_children          (GskRenderNode *node);

GDK_AVAILABLE_IN_3_90
gboolean                gsk_render_node_contains                (GskRenderNode *node,
								 GskRenderNode *descendant);

GDK_AVAILABLE_IN_3_90
void                    gsk_render_node_set_bounds              (GskRenderNode            *node,
                                                                 const graphene_rect_t    *bounds);
GDK_AVAILABLE_IN_3_90
void                    gsk_render_node_set_transform           (GskRenderNode            *node,
                                                                 const graphene_matrix_t  *transform);
GDK_AVAILABLE_IN_3_90
void                    gsk_render_node_set_opacity             (GskRenderNode *node,
                                                                 double         opacity);

GDK_AVAILABLE_IN_3_90
void                    gsk_render_node_set_blend_mode          (GskRenderNode *node,
                                                                 GskBlendMode   blend_mode);

GDK_AVAILABLE_IN_3_90
void                    gsk_render_node_set_scaling_filter      (GskRenderNode *node,
                                                                 GskScalingFilter min_filter,
                                                                 GskScalingFilter mag_filter);

GDK_AVAILABLE_IN_3_90
void                    gsk_render_node_set_name                (GskRenderNode *node,
                                                                 const char    *name);
GDK_AVAILABLE_IN_3_90
const char *            gsk_render_node_get_name                (GskRenderNode *node);

G_END_DECLS

#endif /* __GSK_RENDER_NODE_H__ */

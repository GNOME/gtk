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

/**
 * GskRenderNodeReplayNodeFilter:
 * @replay: The replay object used to replay the node
 * @node: The node to replay
 * @user_data: The user data
 *
 * A function to replay a node.
 *
 * The node may be returned unmodified.
 *
 * The node may be discarded by returning %NULL.
 *
 * If you do not want to do any handling yourself, call
 * [method@Gsk.RenderNodeReplay.default] to use the default handler
 * that calls your function on the children of the node.
 *
 * Returns: (transfer full) (nullable): The replayed node
 *
 * Since: 4.22
 */
typedef GskRenderNode *(* GskRenderNodeReplayNodeFilter)                (GskRenderNodeReplay            *replay,
                                                                         GskRenderNode                  *node,
                                                                         gpointer                        user_data);

/**
 * GskRenderNodeReplayNodeForeach:
 * @replay: The replay object used to replay the node
 * @node: The node to replay
 * @user_data: The user data
 *
 * A function called for every node.
 *
 * If this function returns TRUE, the replay will continue and call
 * the filter function.
 *
 * If it returns FALSE, the filter function will not be called and
 * any child nodes will be skipped.
 *
 * Returns: TRUE to descend into this node's child nodes (if any)
 *   or FALSE to skip them
 *
 * Since: 4.22
 */
typedef gboolean        (* GskRenderNodeReplayNodeForeach)              (GskRenderNodeReplay            *replay,
                                                                         GskRenderNode                  *node,
                                                                         gpointer                        user_data);

/**
 * GskRenderNodeReplayTextureFilter:
 * @replay: The replay object used to replay the node
 * @node: The node the texture belongs to
 * @texture: The texture to filter
 * @user_data: The user data
 *
 * A function that filters textures.
 *
 * The function will be called by the default replay function for
 * all nodes with textures. They will then generate a node using the
 * returned texture.
 *
 * It is valid for the function to return the passed in texture if
 * the texture shuld not be modified.
 *
 * Returns: (transfer full): The filtered texture
 *
 * Since: 4.22
 */
typedef GdkTexture *   (* GskRenderNodeReplayTextureFilter)             (GskRenderNodeReplay            *replay,
                                                                         GskRenderNode                  *node,
                                                                         GdkTexture                     *texture,
                                                                         gpointer                        user_data);

GDK_AVAILABLE_IN_4_22
GskRenderNodeReplay *   gsk_render_node_replay_new                      (void);
GDK_AVAILABLE_IN_4_22
void                    gsk_render_node_replay_free                     (GskRenderNodeReplay            *self);

GDK_AVAILABLE_IN_4_22
void                    gsk_render_node_replay_set_node_filter          (GskRenderNodeReplay            *self,
                                                                         GskRenderNodeReplayNodeFilter   filter,
                                                                         gpointer                        user_data,
                                                                         GDestroyNotify                  user_destroy);
GDK_AVAILABLE_IN_4_22
GskRenderNode *         gsk_render_node_replay_filter_node              (GskRenderNodeReplay            *self,
                                                                         GskRenderNode                  *node);
GDK_AVAILABLE_IN_4_22
GskRenderNode *         gsk_render_node_replay_default                  (GskRenderNodeReplay            *self,
                                                                         GskRenderNode                  *node);

GDK_AVAILABLE_IN_4_22
void                    gsk_render_node_replay_set_node_foreach         (GskRenderNodeReplay            *self,
                                                                         GskRenderNodeReplayNodeForeach  foreach,
                                                                         gpointer                        user_data,
                                                                         GDestroyNotify                  user_destroy);
GDK_AVAILABLE_IN_4_22
void                    gsk_render_node_replay_foreach_node             (GskRenderNodeReplay            *self,
                                                                         GskRenderNode                  *node);


GDK_AVAILABLE_IN_4_22
void                    gsk_render_node_replay_set_texture_filter       (GskRenderNodeReplay            *self,
                                                                         GskRenderNodeReplayTextureFilter filter,
                                                                         gpointer                        user_data,
                                                                         GDestroyNotify                  user_destroy);
GDK_AVAILABLE_IN_4_22
GdkTexture *            gsk_render_node_replay_filter_texture           (GskRenderNodeReplay            *self,
                                                                         GskRenderNode                  *node,
                                                                         GdkTexture                     *texture);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskRenderNodeReplay, gsk_render_node_replay_free)

G_END_DECLS


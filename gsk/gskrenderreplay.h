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
 * GskRenderReplayNodeFilter:
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
 * [method@Gsk.RenderReplay.default] to use the default handler
 * that calls your function on the children of the node.
 *
 * Returns: (transfer full) (nullable): The replayed node
 *
 * Since: 4.22
 */
typedef GskRenderNode * (* GskRenderReplayNodeFilter)           (GskRenderReplay                *replay,
                                                                 GskRenderNode                  *node,
                                                                 gpointer                        user_data);

GDK_AVAILABLE_IN_4_22
GskRenderReplay *       gsk_render_replay_new                   (void);
GDK_AVAILABLE_IN_4_22
void                    gsk_render_replay_free                  (GskRenderReplay                *self);

GDK_AVAILABLE_IN_4_22
void                    gsk_render_replay_set_node_filter       (GskRenderReplay                *self,
                                                                 GskRenderReplayNodeFilter       filter,
                                                                 gpointer                        user_data,
                                                                 GDestroyNotify                  user_destroy);
GDK_AVAILABLE_IN_4_22
GskRenderNode *         gsk_render_replay_filter_node           (GskRenderReplay                *self,
                                                                 GskRenderNode                  *node);
GDK_AVAILABLE_IN_4_22
GskRenderNode *         gsk_render_replay_default               (GskRenderReplay                *self,
                                                                 GskRenderNode                  *node);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskRenderReplay, gsk_render_replay_free)

G_END_DECLS


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

#include "config.h"

#include "gskrendernodereplay.h"

#include "gskrendernodeprivate.h"

/**
 * GskRenderNodeReplay: (free-func gsk_render_node_replay_free)
 *
 * A method to replay a [class@Gsk.RenderNode] and its children, potentially
 * modifying them.
 *
 * This is a utility tool to walk a rendernode tree. The most powerful way
 * is to provide a function via [method@Gsk.RenderNodeReplay.set_node_filter]
 * to filter each individual node and then run
 * [method@Gsk.RenderNodeReplay.filter_node] on the nodes you want to filter.
 *
 * Since: 4.22
 */

struct _GskRenderNodeReplay
{
  GskRenderNodeReplayNodeFilter node_filter;
  gpointer node_filter_data;
  GDestroyNotify node_filter_destroy;
};

/**
 * gsk_render_node_replay_new:
 *
 * Creates a new replay object to replay nodes.
 *
 * Returns: A new replay object to replay nodes
 *
 * Since: 4.22
 **/
GskRenderNodeReplay *
gsk_render_node_replay_new (void)
{
  GskRenderNodeReplay *self;

  self = g_new0 (GskRenderNodeReplay, 1);

  return self;
}

/**
 * gsk_render_node_replay_free:
 * @self: the replay object to free
 *
 * Frees a `GskRenderNodeReplay`.
 *
 * Since: 4.22
 **/
void
gsk_render_node_replay_free (GskRenderNodeReplay *self)
{
  g_return_if_fail (self != NULL);

  gsk_render_node_replay_set_node_filter (self, NULL, NULL, NULL);

  g_free (self);
}

/**
 * gsk_render_node_replay_set_node_filter:
 * @filter: The function to call to replay nodes
 * @user_data: user data to pass to @func
 * @user_destroy: destroy notify that will be called to release
 *   user_data
 *
 * Sets the function to use as a node filter. This is the most complex
 * function to use for replaying nodes. This function can either:
 *
 * * keep the node and just return itself
 *
 * * create a replacement node and return that
 *
 * * discard the node by returning %NULL
 *
 * * call [method@Gsk.RenderNodeReplay.default] to have the default handler
 *   run for this node, which calls your function on its children
 *
 * Since: 4.22
 */
void
gsk_render_node_replay_set_node_filter (GskRenderNodeReplay           *self,
                                        GskRenderNodeReplayNodeFilter  filter,
                                        gpointer                       user_data,
                                        GDestroyNotify                 user_destroy)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (filter || user_data == NULL);
  g_return_if_fail (user_data || !user_destroy);

  if (self->node_filter_destroy)
    self->node_filter_destroy (self->node_filter_data);

  self->node_filter = filter;
  self->node_filter_data = user_data;
  self->node_filter_destroy = user_destroy;
}

/**
 * gsk_render_node_replay_filter_node:
 * @self: the replay
 * @node: the node to replay
 *
 * Replays a node using the replay's filter function.
 *
 * After the replay the node may be unchanged, or it may be
 * removed, which will result in %NULL being returned.
 *
 * Returns: (transfer full) (nullable): The replayed node
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_render_node_replay_filter_node (GskRenderNodeReplay *self,
                                    GskRenderNode       *node)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  if (self->node_filter)
    return self->node_filter (self, node, self->node_filter_data);
  else
    return gsk_render_node_replay_default (self, node);
}

/**
 * gsk_render_node_replay_default:
 * @self: the replay
 * @node: the node to replay
 *
 * Replays the node using the default method, which means
 * replaying all potential child nodes and creating a node
 * with the replayed children and otherwise unchanged
 * properties.
 *
 * If the node has no children or no child was changed,
 * the node itself may be returned.
 *
 * Returns: (transfer full) (nullable): The replayed node 
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_render_node_replay_default (GskRenderNodeReplay *self,
                                GskRenderNode       *node)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return GSK_RENDER_NODE_GET_CLASS (node)->replay (node, self);
}

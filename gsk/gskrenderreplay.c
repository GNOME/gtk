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

#include "gskrenderreplay.h"

#include "gskrendernodeprivate.h"

/**
 * GskRenderReplay: (free-func gsk_render_replay_free)
 *
 * A facility to replay a [class@Gsk.RenderNode] and its children, potentially
 * modifying them.
 *
 * This is a utility tool to walk a rendernode tree. The most powerful way
 * is to provide a function via [method@Gsk.RenderReplay.set_node_filter]
 * to filter each individual node and then run
 * [method@Gsk.RenderReplay.filter_node] on the nodes you want to filter.
 *
 * Since: 4.22
 */

struct _GskRenderReplay
{
  GskRenderReplayNodeFilter node_filter;
  gpointer node_filter_data;
  GDestroyNotify node_filter_destroy;
};

/**
 * gsk_render_replay_new:
 *
 * Creates a new replay object to replay nodes.
 *
 * Returns: A new replay object to replay nodes
 *
 * Since: 4.22
 **/
GskRenderReplay *
gsk_render_replay_new (void)
{
  GskRenderReplay *self;

  self = g_new0 (GskRenderReplay, 1);

  return self;
}

/**
 * gsk_render_replay_free:
 * @self: the replay object to free
 *
 * Frees a `GskRenderReplay`.
 *
 * Since: 4.22
 **/
void
gsk_render_replay_free (GskRenderReplay *self)
{
  g_return_if_fail (self != NULL);

  gsk_render_replay_set_node_filter (self, NULL, NULL, NULL);

  g_free (self);
}

/**
 * gsk_render_replay_set_node_filter:
 * @filter: The function to call to replay nodes
 * @user_data: user data to pass to @func
 * @user_destroy: destroy notify that will be called to release
 *   user_data
 *
 * Sets the function to use as a node filter.
 *
 * This is the most complex function to use for replaying nodes.
 * It can either:
 *
 * * keep the node and just return it unchanged
 *
 * * create a replacement node and return that
 *
 * * discard the node by returning %NULL
 *
 * * call [method@Gsk.RenderReplay.default] to have the default handler
 *   run for this node, which calls your function on its children
 *
 * Since: 4.22
 */
void
gsk_render_replay_set_node_filter (GskRenderReplay           *self,
                                   GskRenderReplayNodeFilter  filter,
                                   gpointer                   user_data,
                                   GDestroyNotify             user_destroy)
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
 * gsk_render_replay_filter_node:
 * @self: the replay
 * @node: the node to replay
 *
 * Replays a node using the replay's filter function.
 *
 * After the replay the node may be unchanged, or it may be
 * removed, which will result in %NULL being returned.
 *
 * This function calls the registered callback in the following order:
 *
 * 1. If a foreach function is set, it is called first. If it returns 
 *    false, this function immediately exits and returns the passed
 *    in node.
 *
 * 2. If a node filter is set, it is called and its result is returned.
 *
 * 3. [method@Gsk.RenderReplay.default] is called and its result is
 *    returned.
 *
 * Returns: (transfer full) (nullable): The replayed node
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_render_replay_filter_node (GskRenderReplay *self,
                               GskRenderNode   *node)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  if (self->node_filter)
    return self->node_filter (self, node, self->node_filter_data);
  else
    return gsk_render_replay_default (self, node);
}

/**
 * gsk_render_replay_default:
 * @self: the replay
 * @node: the node to replay
 *
 * Replays the node using the default method.
 *
 * The default method calls [method@Gsk.RenderReplay.filter_node]
 * on all its child nodes and the filter functions for all its
 * proeprties. If none of them are changed, it returns the passed
 * in node. Otherwise it constructs a new node with the changed
 * children and properties.
 *
 * It may not be possible to construct a new node when any of the
 * callbacks return NULL. In that case, this function will return
 * NULL, too.
 *
 * Returns: (transfer full) (nullable): The replayed node 
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_render_replay_default (GskRenderReplay *self,
                           GskRenderNode   *node)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return GSK_RENDER_NODE_GET_CLASS (node)->replay (node, self);
}

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

#include "config.h"

#include "gskdebugnode.h"

#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

/**
 * GskDebugNode:
 *
 * A render node that emits a debugging message when drawing its
 * child node.
 */
struct _GskDebugNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  char *message;
};

static void
gsk_debug_node_finalize (GskRenderNode *node)
{
  GskDebugNode *self = (GskDebugNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_DEBUG_NODE));

  gsk_render_node_unref (self->child);
  g_free (self->message);

  parent_class->finalize (node);
}

static void
gsk_debug_node_draw (GskRenderNode *node,
                     cairo_t       *cr,
                     GdkColorState *ccs)
{
  GskDebugNode *self = (GskDebugNode *) node;

  gsk_render_node_draw_ccs (self->child, cr, ccs);
}

static gboolean
gsk_debug_node_can_diff (const GskRenderNode *node1,
                         const GskRenderNode *node2)
{
  GskDebugNode *self1 = (GskDebugNode *) node1;
  GskDebugNode *self2 = (GskDebugNode *) node2;

  return gsk_render_node_can_diff (self1->child, self2->child);
}

static void
gsk_debug_node_diff (GskRenderNode *node1,
                     GskRenderNode *node2,
                     GskDiffData   *data)
{
  GskDebugNode *self1 = (GskDebugNode *) node1;
  GskDebugNode *self2 = (GskDebugNode *) node2;

  gsk_render_node_diff (self1->child, self2->child, data);
}

static gboolean
gsk_debug_node_get_opaque_rect (GskRenderNode   *node,
                                graphene_rect_t *out_opaque)
{
  GskDebugNode *self = (GskDebugNode *) node;

  return gsk_render_node_get_opaque_rect (self->child, out_opaque);
}

static GskRenderNode *
gsk_debug_node_replay (GskRenderNode   *node,
                       GskRenderReplay *replay)
{
  GskDebugNode *self = (GskDebugNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_debug_node_new (child, self->message);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_debug_node_class_init (gpointer g_class,
                           gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_DEBUG_NODE;

  node_class->finalize = gsk_debug_node_finalize;
  node_class->draw = gsk_debug_node_draw;
  node_class->can_diff = gsk_debug_node_can_diff;
  node_class->diff = gsk_debug_node_diff;
  node_class->replay = gsk_debug_node_replay;
  node_class->get_opaque_rect = gsk_debug_node_get_opaque_rect;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskDebugNode, gsk_debug_node)

/**
 * gsk_debug_node_new:
 * @child: The child to add debug info for
 * @message: (transfer full): The debug message
 *
 * Creates a `GskRenderNode` that will add debug information about
 * the given @child.
 *
 * Adding this node has no visual effect.
 *
 * Returns: (transfer full) (type GskDebugNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_debug_node_new (GskRenderNode *child,
                    char          *message)
{
  GskDebugNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_DEBUG_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = child->fully_opaque;

  self->child = gsk_render_node_ref (child);
  self->message = message;

  gsk_rect_init_from_rect (&node->bounds, &child->bounds);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  self->render_node.is_hdr = gsk_render_node_is_hdr (child);

  return node;
}

/**
 * gsk_debug_node_get_child:
 * @node: (type GskDebugNode): a debug `GskRenderNode`
 *
 * Gets the child node that is getting drawn by the given @node.
 *
 * Returns: (transfer none): the child `GskRenderNode`
 **/
GskRenderNode *
gsk_debug_node_get_child (const GskRenderNode *node)
{
  const GskDebugNode *self = (const GskDebugNode *) node;

  return self->child;
}

/**
 * gsk_debug_node_get_message:
 * @node: (type GskDebugNode): a debug `GskRenderNode`
 *
 * Gets the debug message that was set on this node
 *
 * Returns: (transfer none): The debug message
 **/
const char *
gsk_debug_node_get_message (const GskRenderNode *node)
{
  const GskDebugNode *self = (const GskDebugNode *) node;

  return self->message;
}


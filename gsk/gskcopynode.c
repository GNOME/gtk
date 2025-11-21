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

#include "gskcopynode.h"

#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

/**
 * GskCopyNode:
 *
 * A render node that copies the current state of the rendering canvas
 * so a [class@Gsk.PasteNode] can draw it.
 *
 * Since: 4.22
 */
struct _GskCopyNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
};

static void
gsk_copy_node_finalize (GskRenderNode *node)
{
  GskCopyNode *self = (GskCopyNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_COPY_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
gsk_copy_node_draw (GskRenderNode *node,
                    cairo_t       *cr,
                    GdkColorState *ccs)
{
  GskCopyNode *self = (GskCopyNode *) node;

  gsk_render_node_draw_ccs (self->child, cr, ccs);
}

static void
gsk_copy_node_diff (GskRenderNode *node1,
                    GskRenderNode *node2,
                    GskDiffData   *data)
{
  GskCopyNode *self1 = (GskCopyNode *) node1;
  GskCopyNode *self2 = (GskCopyNode *) node2;

  gsk_render_node_diff (self1->child, self2->child, data);
}

static gboolean
gsk_copy_node_get_opaque_rect (GskRenderNode   *node,
                               graphene_rect_t *out_opaque)
{
  GskCopyNode *self = (GskCopyNode *) node;

  return gsk_render_node_get_opaque_rect (self->child, out_opaque);
}

static GskRenderNode *
gsk_copy_node_replay (GskRenderNode   *node,
                      GskRenderReplay *replay)
{
  GskCopyNode *self = (GskCopyNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_copy_node_new (child);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_copy_node_class_init (gpointer g_class,
                           gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_COPY_NODE;

  node_class->finalize = gsk_copy_node_finalize;
  node_class->draw = gsk_copy_node_draw;
  node_class->diff = gsk_copy_node_diff;
  node_class->replay = gsk_copy_node_replay;
  node_class->get_opaque_rect = gsk_copy_node_get_opaque_rect;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskCopyNode, gsk_copy_node)

/**
 * gsk_copy_node_new:
 * @child: The child
 *
 * Creates a `GskRenderNode` that copies the current rendering
 * canvas for playback by paste nodes that are part of the child.
 *
 * Returns: (transfer full) (type GskCopyNode): A new `GskRenderNode`
 *
 * Since: 4.22
 */
GskRenderNode *
gsk_copy_node_new (GskRenderNode *child)
{
  GskCopyNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_COPY_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = child->fully_opaque;

  self->child = gsk_render_node_ref (child);

  gsk_rect_init_from_rect (&node->bounds, &child->bounds);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->clears_background = gsk_render_node_clears_background (child);
  node->copy_mode = GSK_COPY_ANY;

  return node;
}

/**
 * gsk_copy_node_get_child:
 * @node: (type GskCopyNode): a copy `GskRenderNode`
 *
 * Gets the child node that is getting drawn by the given @node.
 *
 * Returns: (transfer none): the child `GskRenderNode`
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_copy_node_get_child (const GskRenderNode *node)
{
  const GskCopyNode *self = (const GskCopyNode *) node;

  return self->child;
}


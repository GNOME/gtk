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

#include "gskpastenode.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"

/**
 * GskPasteNode:
 *
 * A render node for a paste.
 *
 * Since: 4.22
 */
struct _GskPasteNode
{
  GskRenderNode render_node;
  gsize depth;
};

static void
gsk_paste_node_draw (GskRenderNode *node,
                     cairo_t       *cr,
                     GskCairoData  *data)
{
  /* FIXME */
}

static void
gsk_paste_node_diff (GskRenderNode *node1,
                     GskRenderNode *node2,
                     GskDiffData   *data)
{
  GskPasteNode *self1 = (GskPasteNode *) node1;
  GskPasteNode *self2 = (GskPasteNode *) node2;
  const GSList *copy;
  cairo_region_t *sub;
  cairo_rectangle_int_t bounds;

  if (!gsk_rect_equal (&node1->bounds, &node2->bounds) ||
      self1->depth != self2->depth)
    {
      gsk_render_node_diff_impossible (node1, node2, data);
      return;
    }

  copy = g_slist_nth (data->copies, self1->depth);
  if (copy == NULL)
    return;

  sub = cairo_region_copy (copy->data);
  gsk_rect_to_cairo_grow (&node1->bounds, &bounds);
  cairo_region_intersect_rectangle (sub, &bounds);
  cairo_region_union (data->region, sub);
  cairo_region_destroy (sub);
}

static GskRenderNode *
gsk_paste_node_replay (GskRenderNode   *node,
                       GskRenderReplay *replay)
{
  return gsk_render_node_ref (node);
}

static void
gsk_paste_node_render_opacity (GskRenderNode  *node,
                               GskOpacityData *data)
{
  GskPasteNode *self = (GskPasteNode *) node;
  const graphene_rect_t *copy;
  graphene_rect_t clipped;

  copy = g_slist_nth_data (data->copies, self->depth);
  if (copy == NULL)
    return;

  if (gsk_rect_intersection (copy, &node->bounds, &clipped))
    gsk_rect_coverage (&data->opaque, &clipped, &data->opaque);
}

static void
gsk_paste_node_class_init (gpointer g_class,
                           gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_PASTE_NODE;

  node_class->draw = gsk_paste_node_draw;
  node_class->diff = gsk_paste_node_diff;
  node_class->replay = gsk_paste_node_replay;
  node_class->render_opacity = gsk_paste_node_render_opacity;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskPasteNode, gsk_paste_node)

/**
 * gsk_paste_node_get_depth:
 * @node: (type GskPasteNode): a `GskRenderNode`
 *
 * Retrieves the index of the copy that should be pasted.
 *
 * Returns: the index of the copy to paste.
 *
 * Since: 4.22
 */
gsize
gsk_paste_node_get_depth (const GskRenderNode *node)
{
  GskPasteNode *self = (GskPasteNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_PASTE_NODE), 0);

  return self->depth;
}

/**
 * gsk_paste_node_new:
 * @bounds: the rectangle to render the paste into
 * @depth: the index of which copy to paste. This will usually be 0.
 *
 * Creates a `GskRenderNode` that will paste copied contents.
 *
 * Returns: (transfer full) (type GskPasteNode): A new `GskRenderNode`
 *
 * Since: 4.22
 */
GskRenderNode *
gsk_paste_node_new (const graphene_rect_t *bounds,
                    gsize                  depth)
{
  GskPasteNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_PASTE_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = FALSE;
  node->preferred_depth = GDK_MEMORY_NONE;
  node->contains_paste_node = TRUE;

  self->depth = depth;

  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);

  return node;
}


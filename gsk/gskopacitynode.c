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

#include "gskopacitynode.h"

#include "gskrectprivate.h"
#include "gskrenderreplay.h"
#include "gskrendernodeprivate.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskOpacityNode:
 *
 * A render node controlling the opacity of its single child node.
 */
struct _GskOpacityNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  float opacity;
};

static void
gsk_opacity_node_finalize (GskRenderNode *node)
{
  GskOpacityNode *self = (GskOpacityNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_OPACITY_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
gsk_opacity_node_draw (GskRenderNode *node,
                       cairo_t       *cr,
                       GskCairoData  *data)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  /* clip so the push_group() creates a smaller surface */
  gdk_cairo_rectangle_snap_to_grid (cr, &node->bounds);
  cairo_clip (cr);

  if (gdk_cairo_is_all_clipped (cr))
    return;

  cairo_push_group (cr);

  gsk_render_node_draw_full (self->child, cr, data);

  cairo_pop_group_to_source (cr);
  cairo_paint_with_alpha (cr, self->opacity);
}

static void
gsk_opacity_node_diff (GskRenderNode *node1,
                       GskRenderNode *node2,
                       GskDiffData   *data)
{
  GskOpacityNode *self1 = (GskOpacityNode *) node1;
  GskOpacityNode *self2 = (GskOpacityNode *) node2;

  if (self1->opacity == self2->opacity)
    gsk_render_node_diff (self1->child, self2->child, data);
  else
    gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode **
gsk_opacity_node_get_children (GskRenderNode *node,
                               gsize         *n_children)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  *n_children = 1;
  
  return &self->child;
}

static GskRenderNode *
gsk_opacity_node_replay (GskRenderNode   *node,
                         GskRenderReplay *replay)
{
  GskOpacityNode *self = (GskOpacityNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_opacity_node_new (child, self->opacity);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_opacity_node_class_init (gpointer g_class,
                             gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_OPACITY_NODE;

  node_class->finalize = gsk_opacity_node_finalize;
  node_class->draw = gsk_opacity_node_draw;
  node_class->diff = gsk_opacity_node_diff;
  node_class->get_children = gsk_opacity_node_get_children;
  node_class->replay = gsk_opacity_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskOpacityNode, gsk_opacity_node)

/**
 * gsk_opacity_node_new:
 * @child: The node to draw
 * @opacity: The opacity to apply
 *
 * Creates a `GskRenderNode` that will drawn the @child with reduced
 * @opacity.
 *
 * Returns: (transfer full) (type GskOpacityNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_opacity_node_new (GskRenderNode *child,
                      float          opacity)
{
  GskOpacityNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_OPACITY_NODE);
  node = (GskRenderNode *) self;

  self->child = gsk_render_node_ref (child);
  self->opacity = CLAMP (opacity, 0.0, 1.0);

  gsk_rect_init_from_rect (&node->bounds, &child->bounds);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);

  return node;
}

/**
 * gsk_opacity_node_get_child:
 * @node: (type GskOpacityNode): a `GskRenderNode` for an opacity
 *
 * Gets the child node that is getting opacityed by the given @node.
 *
 * Returns: (transfer none): The child that is getting opacityed
 */
GskRenderNode *
gsk_opacity_node_get_child (const GskRenderNode *node)
{
  const GskOpacityNode *self = (const GskOpacityNode *) node;

  return self->child;
}

/**
 * gsk_opacity_node_get_opacity:
 * @node: (type GskOpacityNode): a `GskRenderNode` for an opacity
 *
 * Gets the transparency factor for an opacity node.
 *
 * Returns: the opacity factor
 */
float
gsk_opacity_node_get_opacity (const GskRenderNode *node)
{
  const GskOpacityNode *self = (const GskOpacityNode *) node;

  return self->opacity;
}

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

#include "gskfillnode.h"

#include "gskcolornodeprivate.h"
#include "gskpath.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

#include <gdk/gdkcairoprivate.h>

/**
 * GskFillNode:
 *
 * A render node filling the area given by [struct@Gsk.Path]
 * and [enum@Gsk.FillRule] with the child node.
 *
 * Since: 4.14
 */

struct _GskFillNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GskPath *path;
  GskFillRule fill_rule;
};

static void
gsk_fill_node_finalize (GskRenderNode *node)
{
  GskFillNode *self = (GskFillNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_FILL_NODE));

  gsk_render_node_unref (self->child);
  gsk_path_unref (self->path);

  parent_class->finalize (node);
}

static void
gsk_fill_node_draw (GskRenderNode *node,
                    cairo_t       *cr,
                    GskCairoData  *data)
{
  GskFillNode *self = (GskFillNode *) node;

  switch (self->fill_rule)
  {
    case GSK_FILL_RULE_WINDING:
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING);
      break;
    case GSK_FILL_RULE_EVEN_ODD:
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  gsk_path_to_cairo (self->path, cr);
  if (gsk_render_node_get_node_type (self->child) == GSK_COLOR_NODE &&
      gsk_rect_contains_rect (&self->child->bounds, &node->bounds))
    {
      gdk_cairo_set_source_rgba_ccs (cr, data->ccs, gsk_color_node_get_color (self->child));
      cairo_fill (cr);
    }
  else
    {
      cairo_clip (cr);
      gsk_render_node_draw_full (self->child, cr, data);
    }
}

static void
gsk_fill_node_diff (GskRenderNode *node1,
                    GskRenderNode *node2,
                    GskDiffData   *data)
{
  GskFillNode *self1 = (GskFillNode *) node1;
  GskFillNode *self2 = (GskFillNode *) node2;

  if (self1->path == self2->path)
    {
      cairo_region_t *save;
      cairo_rectangle_int_t clip_rect;

      save = cairo_region_copy (data->region);
      gsk_render_node_diff (self1->child, self2->child, data);
      gsk_rect_to_cairo_grow (&node1->bounds, &clip_rect);
      cairo_region_intersect_rectangle (data->region, &clip_rect);
      cairo_region_union (data->region, save);
      cairo_region_destroy (save);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static GskRenderNode **
gsk_fill_node_get_children (GskRenderNode *node,
                            gsize         *n_children)
{
  GskFillNode *self = (GskFillNode *) node;

  *n_children = 1;

  return &self->child;
}

static GskRenderNode *
gsk_fill_node_replay (GskRenderNode   *node,
                      GskRenderReplay *replay)
{
  GskFillNode *self = (GskFillNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_fill_node_new (child, self->path, self->fill_rule);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_fill_node_class_init (gpointer g_class,
                          gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_FILL_NODE;

  node_class->finalize = gsk_fill_node_finalize;
  node_class->draw = gsk_fill_node_draw;
  node_class->diff = gsk_fill_node_diff;
  node_class->get_children = gsk_fill_node_get_children;
  node_class->replay = gsk_fill_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskFillNode, gsk_fill_node)

/**
 * gsk_fill_node_new:
 * @child: The node to fill the area with
 * @path: The path describing the area to fill
 * @fill_rule: The fill rule to use
 *
 * Creates a `GskRenderNode` that will fill the @child in the area
 * given by @path and @fill_rule.
 *
 * Returns: (transfer none) (type GskFillNode): A new `GskRenderNode`
 *
 * Since: 4.14
 */
GskRenderNode *
gsk_fill_node_new (GskRenderNode *child,
                   GskPath       *path,
                   GskFillRule    fill_rule)
{
  GskFillNode *self;
  GskRenderNode *node;
  graphene_rect_t path_bounds;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_FILL_NODE);
  node = (GskRenderNode *) self;
  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->clears_background = gsk_render_node_clears_background (child);
  node->copy_mode = gsk_render_node_get_copy_mode (child);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);
  node->needs_blending = gsk_render_node_needs_blending (child);

  self->child = gsk_render_node_ref (child);
  self->path = gsk_path_ref (path);
  self->fill_rule = fill_rule;

  if (!gsk_path_get_bounds (path, &path_bounds) || 
      !gsk_rect_intersection (&path_bounds, &child->bounds, &node->bounds))
    node->bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);

  return node;
}

/**
 * gsk_fill_node_get_child:
 * @node: (type GskFillNode): a fill `GskRenderNode`
 *
 * Gets the child node that is getting drawn by the given @node.
 *
 * Returns: (transfer none): The child that is getting drawn
 *
 * Since: 4.14
 */
GskRenderNode *
gsk_fill_node_get_child (const GskRenderNode *node)
{
  const GskFillNode *self = (const GskFillNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_FILL_NODE), NULL);

  return self->child;
}

/**
 * gsk_fill_node_get_path:
 * @node: (type GskFillNode): a fill `GskRenderNode`
 *
 * Retrieves the path used to describe the area filled with the contents of
 * the @node.
 *
 * Returns: (transfer none): a `GskPath`
 *
 * Since: 4.14
 */
GskPath *
gsk_fill_node_get_path (const GskRenderNode *node)
{
  const GskFillNode *self = (const GskFillNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_FILL_NODE), NULL);

  return self->path;
}

/**
 * gsk_fill_node_get_fill_rule:
 * @node: (type GskFillNode): a fill `GskRenderNode`
 *
 * Retrieves the fill rule used to determine how the path is filled.
 *
 * Returns: a `GskFillRule`
 *
 * Since: 4.14
 */
GskFillRule
gsk_fill_node_get_fill_rule (const GskRenderNode *node)
{
  const GskFillNode *self = (const GskFillNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_FILL_NODE), GSK_FILL_RULE_WINDING);

  return self->fill_rule;
}


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

#include "gskrepeatnode.h"

#include "gskrectprivate.h"
#include "gskrenderreplay.h"
#include "gskrendernodeprivate.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskRepeatNode:
 *
 * A render node repeating its single child node.
 */
struct _GskRepeatNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  graphene_rect_t child_bounds;
};

static void
gsk_repeat_node_finalize (GskRenderNode *node)
{
  GskRepeatNode *self = (GskRepeatNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_REPEAT_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
gsk_repeat_node_draw_tiled (cairo_t                *cr,
                            GdkColorState          *ccs,
                            const graphene_rect_t  *rect,
                            float                   x,
                            float                   y,
                            GskRenderNode          *child,
                            const graphene_rect_t  *child_bounds)
{
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;

  cairo_save (cr);
  /* reset the clip so we get an unclipped pattern for repeating */
  cairo_reset_clip (cr);
  cairo_translate (cr,
                   x * child_bounds->size.width,
                   y * child_bounds->size.height);
  gdk_cairo_rect (cr, child_bounds);
  cairo_clip (cr);

  cairo_push_group (cr);
  gsk_render_node_draw_ccs (child, cr, ccs);
  pattern = cairo_pop_group (cr);
  cairo_restore (cr);

  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  cairo_pattern_get_matrix (pattern, &matrix);
  cairo_matrix_translate (&matrix,
                          - x * child_bounds->size.width,
                          - y * child_bounds->size.height);
  cairo_pattern_set_matrix (pattern, &matrix);
  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  gdk_cairo_rect (cr, rect);
  cairo_fill (cr);
}

static void
gsk_repeat_node_draw (GskRenderNode *node,
                      cairo_t       *cr,
                      GdkColorState *ccs)
{
  GskRepeatNode *self = (GskRepeatNode *) node;
  graphene_rect_t clip_bounds;
  float tile_left, tile_right, tile_top, tile_bottom;

  gdk_cairo_rectangle_snap_to_grid (cr, &node->bounds);
  cairo_clip (cr);
  _graphene_rect_init_from_clip_extents (&clip_bounds, cr);

  tile_left = (clip_bounds.origin.x - self->child_bounds.origin.x) / self->child_bounds.size.width;
  tile_right = (clip_bounds.origin.x + clip_bounds.size.width - self->child_bounds.origin.x) / self->child_bounds.size.width;
  tile_top = (clip_bounds.origin.y - self->child_bounds.origin.y) / self->child_bounds.size.height;
  tile_bottom = (clip_bounds.origin.y + clip_bounds.size.height - self->child_bounds.origin.y) / self->child_bounds.size.height;

  /* the 1st check tests that a tile fully fits into the bounds,
   * the 2nd check is to catch the case where it fits exactly */
  if (ceilf (tile_left) < floorf (tile_right) &&
      clip_bounds.size.width > self->child_bounds.size.width)
    {
      if (ceilf (tile_top) < floorf (tile_bottom) &&
          clip_bounds.size.height > self->child_bounds.size.height)
        {
          /* tile in both directions */
          gsk_repeat_node_draw_tiled (cr,
                                      ccs,
                                      &clip_bounds,
                                      ceilf (tile_left),
                                      ceilf (tile_top),
                                      self->child,
                                      &self->child_bounds);
        }
      else
        {
          /* tile horizontally, repeat vertically */
          float y;
          for (y = floorf (tile_top); y < ceilf (tile_bottom); y++)
            {
              float start_y = MAX (clip_bounds.origin.y,
                                   self->child_bounds.origin.y + y * self->child_bounds.size.height);
              float end_y = MAX (clip_bounds.origin.y + clip_bounds.size.height,
                                 self->child_bounds.origin.y + (y + 1) * self->child_bounds.size.height);
              gsk_repeat_node_draw_tiled (cr,
                                          ccs,
                                          &GRAPHENE_RECT_INIT (
                                              clip_bounds.origin.x,
                                              start_y,
                                              clip_bounds.size.width,
                                              end_y - start_y
                                          ),
                                          ceilf (tile_left),
                                          y,
                                          self->child,
                                          &self->child_bounds);
            }
        }
    }
  else if (ceilf (tile_top) < floorf (tile_bottom) &&
           clip_bounds.size.height > self->child_bounds.size.height)
    {
      /* repeat horizontally, tile vertically */
      float x;
      for (x = floorf (tile_left); x < ceilf (tile_right); x++)
        {
          float start_x = MAX (clip_bounds.origin.x,
                               self->child_bounds.origin.x + x * self->child_bounds.size.width);
          float end_x = MAX (clip_bounds.origin.x + clip_bounds.size.width,
                             self->child_bounds.origin.x + (x + 1) * self->child_bounds.size.width);
          gsk_repeat_node_draw_tiled (cr,
                                      ccs,
                                      &GRAPHENE_RECT_INIT (
                                          start_x,
                                          clip_bounds.origin.y,
                                          end_x - start_x,
                                          clip_bounds.size.height
                                      ),
                                      x,
                                      ceilf (tile_top),
                                      self->child,
                                      &self->child_bounds);
        }
    }
  else
    {
      /* repeat in both directions */
      float x, y;

      for (x = floorf (tile_left); x < ceilf (tile_right); x++)
        {
          for (y = floorf (tile_top); y < ceilf (tile_bottom); y++)
            {
              cairo_save (cr);
              cairo_translate (cr,
                               x * self->child_bounds.size.width,
                               y * self->child_bounds.size.height);
              gdk_cairo_rect (cr, &self->child_bounds);
              cairo_clip (cr);
              gsk_render_node_draw_ccs (self->child, cr, ccs);
              cairo_restore (cr);
            }
        }
    }
}

static void
gsk_repeat_node_diff (GskRenderNode *node1,
                      GskRenderNode *node2,
                      GskDiffData   *data)
{
  GskRepeatNode *self1 = (GskRepeatNode *) node1;
  GskRepeatNode *self2 = (GskRepeatNode *) node2;

  if (gsk_rect_equal (&node1->bounds, &node2->bounds) &&
      gsk_rect_equal (&self1->child_bounds, &self2->child_bounds))
    {
      cairo_region_t *sub;

      sub = cairo_region_create();
      gsk_render_node_diff (self1->child, self2->child, &(GskDiffData) { sub, data->surface });
      if (cairo_region_is_empty (sub))
        {
          cairo_region_destroy (sub);
          return;
        }
      cairo_region_destroy (sub);
    }

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode *
gsk_repeat_node_replay (GskRenderNode   *node,
                        GskRenderReplay *replay)
{
  GskRepeatNode *self = (GskRepeatNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_repeat_node_new (&node->bounds, child, &self->child_bounds);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_repeat_node_class_init (gpointer g_class,
                            gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_REPEAT_NODE;

  node_class->finalize = gsk_repeat_node_finalize;
  node_class->draw = gsk_repeat_node_draw;
  node_class->diff = gsk_repeat_node_diff;
  node_class->replay = gsk_repeat_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskRepeatNode, gsk_repeat_node)

/**
 * gsk_repeat_node_new:
 * @bounds: The bounds of the area to be painted
 * @child: The child to repeat
 * @child_bounds: (nullable): The area of the child to repeat or %NULL to
 *     use the child's bounds
 *
 * Creates a `GskRenderNode` that will repeat the drawing of @child across
 * the given @bounds.
 *
 * Returns: (transfer full) (type GskRepeatNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_repeat_node_new (const graphene_rect_t *bounds,
                     GskRenderNode         *child,
                     const graphene_rect_t *child_bounds)
{
  GskRepeatNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_REPEAT_NODE);
  node = (GskRenderNode *) self;

  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);

  self->child = gsk_render_node_ref (child);

  if (child_bounds)
    {
      gsk_rect_init_from_rect (&self->child_bounds, child_bounds);
      gsk_rect_normalize (&self->child_bounds);
    }
  else
    {
      gsk_rect_init_from_rect (&self->child_bounds, &child->bounds);
    }

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->fully_opaque = child->fully_opaque &&
                       gsk_rect_contains_rect (&child->bounds, &self->child_bounds) &&
                       !gsk_rect_is_empty (&self->child_bounds);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);

  return node;
}

/**
 * gsk_repeat_node_get_child:
 * @node: (type GskRepeatNode): a repeat `GskRenderNode`
 *
 * Retrieves the child of @node.
 *
 * Returns: (transfer none): a `GskRenderNode`
 */
GskRenderNode *
gsk_repeat_node_get_child (const GskRenderNode *node)
{
  const GskRepeatNode *self = (const GskRepeatNode *) node;

  return self->child;
}

/**
 * gsk_repeat_node_get_child_bounds:
 * @node: (type GskRepeatNode): a repeat `GskRenderNode`
 *
 * Retrieves the bounding rectangle of the child of @node.
 *
 * Returns: (transfer none): a bounding rectangle
 */
const graphene_rect_t *
gsk_repeat_node_get_child_bounds (const GskRenderNode *node)
{
  const GskRepeatNode *self = (const GskRepeatNode *) node;

  return &self->child_bounds;
}

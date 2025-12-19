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

#include "gskrepeatnodeprivate.h"

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
  GskRepeat repeat;
};

static void
gsk_repeat_node_finalize (GskRenderNode *node)
{
  GskRepeatNode *self = (GskRepeatNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_REPEAT_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

/* function should be in gdkcairoprivate.h, but headers... */
static void
gdk_cairo_pattern_set_repeat (cairo_pattern_t *pattern,
                              GskRepeat        repeat)
{
  cairo_extend_t extend[] = {
    [GSK_REPEAT_NONE] = CAIRO_EXTEND_NONE,
    [GSK_REPEAT_PAD] = CAIRO_EXTEND_PAD,
    [GSK_REPEAT_REPEAT] = CAIRO_EXTEND_REPEAT,
    [GSK_REPEAT_REFLECT] = CAIRO_EXTEND_REFLECT,
  };

  cairo_pattern_set_extend (pattern, extend[repeat]);
}

static void
gsk_repeat_node_draw_tiled (cairo_t                *cr,
                            GskCairoData           *data,
                            const graphene_rect_t  *rect,
                            GskRepeat               repeat,
                            GskRenderNode          *child,
                            const graphene_rect_t  *child_bounds,
                            const graphene_point_t *pos)
{
  cairo_pattern_t *pattern;
  cairo_surface_t *child_surface;
  cairo_t *child_cr;
  cairo_matrix_t matrix;

  child_surface = gdk_cairo_create_similar_surface (cr,
                                                    CAIRO_CONTENT_COLOR_ALPHA,
                                                    &GRAPHENE_RECT_INIT (
                                                        0, 0,
                                                        child_bounds->size.width,
                                                        child_bounds->size.height));
  if (child_surface == NULL)
    return;
  child_cr = cairo_create (child_surface);
  cairo_translate (child_cr,
                   - child_bounds->origin.x,
                   - child_bounds->origin.y);
  gsk_render_node_draw_full (child, child_cr, data);
  cairo_destroy (child_cr);

  pattern = cairo_pattern_create_for_surface (child_surface);
  cairo_surface_write_to_png (child_surface, "foo.png");
  gdk_cairo_pattern_set_repeat (pattern, repeat);
  cairo_matrix_init_translate (&matrix, -pos->x, -pos->y);
  cairo_pattern_set_matrix (pattern, &matrix);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  gdk_cairo_rect (cr, rect);
  cairo_fill (cr);

  cairo_surface_destroy (child_surface);
}

static void
gsk_repeat_node_draw_none (GskRenderNode *node,
                           cairo_t       *cr,
                           GskCairoData  *data)
{
  GskRepeatNode *self = (GskRepeatNode *) node;

  gdk_cairo_rect (cr, &node->bounds);
  cairo_clip (cr);
  if (!gsk_rect_contains_rect (&self->child_bounds, &self->child->bounds))
    {
      gdk_cairo_rect (cr, &self->child_bounds);
      cairo_clip (cr);
    }
  gsk_render_node_draw_full (self->child, cr, data);
  return;
}

void
gsk_repeat_node_compute_rect_for_pad (const graphene_rect_t *draw_bounds,
                                      const graphene_rect_t *child_bounds,
                                      graphene_rect_t       *result)
{
  result->size.width = MIN (child_bounds->size.width, draw_bounds->size.width);
  if (child_bounds->origin.x + child_bounds->size.width - result->size.width < draw_bounds->origin.x)
    result->origin.x = child_bounds->origin.x + child_bounds->size.width - result->size.width;
  else if (child_bounds->origin.x < draw_bounds->origin.x)
    result->origin.x = draw_bounds->origin.x;
  else
    result->origin.x = child_bounds->origin.x;

  result->size.height = MIN (child_bounds->size.height, draw_bounds->size.height);
  if (child_bounds->origin.y + child_bounds->size.height - result->size.height < draw_bounds->origin.y)
    result->origin.y = child_bounds->origin.y + child_bounds->size.height - result->size.height;
  else if (child_bounds->origin.y < draw_bounds->origin.y)
    result->origin.y = draw_bounds->origin.y;
  else
    result->origin.y = child_bounds->origin.y;
}

static void
gsk_repeat_node_draw_pad (GskRenderNode *node,
                          cairo_t       *cr,
                          GskCairoData  *data)
{
  GskRepeatNode *self = (GskRepeatNode *) node;
  graphene_rect_t clip_bounds, draw_bounds;

  _graphene_rect_init_from_clip_extents (&clip_bounds, cr);
  if (!gsk_rect_intersection (&clip_bounds, &node->bounds, &clip_bounds))
    return;

  gsk_repeat_node_compute_rect_for_pad (&clip_bounds, &self->child_bounds, &draw_bounds);

  gsk_repeat_node_draw_tiled (cr,
                              data,
                              &clip_bounds,
                              self->repeat,
                              self->child,
                              &draw_bounds,
                              &draw_bounds.origin);
}

static void
gsk_repeat_node_draw_repeat (GskRenderNode *node,
                             cairo_t       *cr,
                             GskCairoData  *data)
{
  GskRepeatNode *self = (GskRepeatNode *) node;
  graphene_rect_t clip_bounds;
  float tile_left, tile_right, tile_top, tile_bottom;

  gdk_cairo_rect (cr, &node->bounds);
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
                                      data,
                                      &clip_bounds,
                                      self->repeat,
                                      self->child,
                                      &self->child_bounds,
                                      &self->child_bounds.origin);
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
                                          data,
                                          &GRAPHENE_RECT_INIT (
                                              clip_bounds.origin.x,
                                              start_y,
                                              clip_bounds.size.width,
                                              end_y - start_y
                                          ),
                                          self->repeat,
                                          self->child,
                                          &GRAPHENE_RECT_INIT (
                                              self->child_bounds.origin.x,
                                              start_y - y * self->child_bounds.size.height,
                                              self->child_bounds.size.width,
                                              end_y - start_y
                                          ),
                                          &GRAPHENE_POINT_INIT (
                                            self->child_bounds.origin.x,
                                            start_y
                                          ));
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
          float end_x = MIN (clip_bounds.origin.x + clip_bounds.size.width,
                             self->child_bounds.origin.x + (x + 1) * self->child_bounds.size.width);
          gsk_repeat_node_draw_tiled (cr,
                                      data,
                                      &GRAPHENE_RECT_INIT (
                                          start_x,
                                          clip_bounds.origin.y,
                                          end_x - start_x,
                                          clip_bounds.size.height
                                      ),
                                      self->repeat,
                                      self->child,
                                      &GRAPHENE_RECT_INIT (
                                          start_x - x * self->child_bounds.size.width,
                                          self->child_bounds.origin.y,
                                          end_x - start_x,
                                          self->child_bounds.size.height
                                      ),
                                      &GRAPHENE_POINT_INIT (
                                        start_x,
                                        self->child_bounds.origin.y
                                      ));
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
              gsk_render_node_draw_full (self->child, cr, data);
              cairo_restore (cr);
            }
        }
    }
}

/*<private>
 * gsk_repeat_node_compute_rect_for_reflect:
 * @draw_bounds: the area that should be drawn
 * @child_bounds: the bounds of the child
 * @child_rect: (out caller-allocates): the part of the child that is needed
 * @pos: (out caller-allocates): where to place the top left of the child rect
 *
 * Computes the part of the child bounds that need to be rendered into an offscreen
 * and where to place that so that when rendering it into the passed in draw bounds
 * with REFLECT it will produce the correct output.
 **/
void
gsk_repeat_node_compute_rect_for_reflect (const graphene_rect_t *draw_bounds,
                                          const graphene_rect_t *child_bounds,
                                          graphene_rect_t       *child_rect,
                                          graphene_point_t      *pos)
{
  float tile_left, tile_right, tile_top, tile_bottom;

  tile_left = (draw_bounds->origin.x - child_bounds->origin.x) / child_bounds->size.width;
  tile_right = (draw_bounds->origin.x + draw_bounds->size.width - child_bounds->origin.x) / child_bounds->size.width;
  tile_top = (draw_bounds->origin.y - child_bounds->origin.y) / child_bounds->size.height;
  tile_bottom = (draw_bounds->origin.y + draw_bounds->size.height - child_bounds->origin.y) / child_bounds->size.height;

  if (draw_bounds->size.width >= child_bounds->size.width)
    {
      /* the tile is fully contained at least once */
      child_rect->origin.x = child_bounds->origin.x;
      child_rect->size.width = child_bounds->size.width;
      pos->x = child_rect->origin.x;
    }
  else if (ceilf (tile_left) <= floorf (tile_right))
    {
      /* one side of the tile gets reflected */
      child_rect->size.width = draw_bounds->size.width;
      if (((int) ceilf (tile_left)) % 2)
        {
          /* ...normal | mirrored... */
          child_rect->origin.x = child_bounds->origin.x + child_bounds->size.width - child_rect->size.width;
          pos->x = child_bounds->origin.x + ceilf (tile_left) * child_bounds->size.width - child_rect->size.width;
        }
      else
        {
          /* ...mirrored | normal... */
          child_rect->origin.x = child_bounds->origin.x;
          pos->x = child_rect->origin.x + ceilf (tile_left) * child_bounds->size.width;
        }
    }
  else
    {
      /* a middle part of the tile is visible */
      float steps = floorf (tile_left);
      child_rect->size.width = draw_bounds->size.width;
      child_rect->origin.x = child_bounds->origin.x + (tile_left - steps) * child_bounds->size.width;
      pos->x = child_rect->origin.x + steps * child_bounds->size.width;
      if ((int) steps % 2)
        {
          child_rect->origin.x = child_bounds->origin.x + (1 - tile_left + steps) * child_bounds->size.width - child_rect->size.width;
          pos->x -= child_rect->size.width;
        }
    }
  
  if (draw_bounds->size.height >= child_bounds->size.height)
    {
      /* the tile is fully contained at least once */
      child_rect->origin.y = child_bounds->origin.y;
      child_rect->size.height = child_bounds->size.height;
      pos->y = child_rect->origin.y;
    }
  else if (ceilf (tile_top) <= floorf (tile_bottom))
    {
      /* one side of the tile gets reflected */
      child_rect->size.height = draw_bounds->size.height;
      if (((int) ceilf (tile_top)) % 2)
        {
          /* ...normal | mirrored... */
          child_rect->origin.y = child_bounds->origin.y + child_bounds->size.height - child_rect->size.height;
          pos->y = child_bounds->origin.y + ceilf (tile_top) * child_bounds->size.height - child_rect->size.height;
        }
      else
        {
          /* ...mirrored | normal... */
          child_rect->origin.y = child_bounds->origin.y;
          pos->y = child_rect->origin.y + ceilf (tile_top) * child_bounds->size.height;
        }
    }
  else
    {
      /* a middle part of the tile is visible */
      float steps = floorf (tile_top);
      child_rect->size.height = draw_bounds->size.height;
      child_rect->origin.y = child_bounds->origin.y + (tile_top - steps) * child_bounds->size.height;
      pos->y = child_rect->origin.y + steps * child_bounds->size.height;
      if ((int) steps % 2)
        {
          child_rect->origin.y = child_bounds->origin.y + (1 - tile_top + steps) * child_bounds->size.height - child_rect->size.height;
          pos->y -= child_rect->size.height;
        }
    }
}

static void
gsk_repeat_node_draw_reflect (GskRenderNode *node,
                              cairo_t       *cr,
                              GskCairoData  *data)
{
  GskRepeatNode *self = (GskRepeatNode *) node;
  graphene_rect_t clip_bounds, draw_bounds;
  graphene_point_t draw_pos;

  gdk_cairo_rect (cr, &node->bounds);
  cairo_clip (cr);
  _graphene_rect_init_from_clip_extents (&clip_bounds, cr);

  gsk_repeat_node_compute_rect_for_reflect (&clip_bounds,
                                            &self->child_bounds,
                                            &draw_bounds,
                                            &draw_pos);

  gsk_repeat_node_draw_tiled (cr,
                              data,
                              &clip_bounds,
                              self->repeat,
                              self->child,
                              &draw_bounds,
                              &draw_pos);
}

static void
gsk_repeat_node_draw (GskRenderNode *node,
                      cairo_t       *cr,
                      GskCairoData  *data)
{
  GskRepeatNode *self = (GskRepeatNode *) node;

  switch (self->repeat)
    {
      case GSK_REPEAT_NONE:
        gsk_repeat_node_draw_none (node, cr, data);
        break;

      case GSK_REPEAT_PAD:
        gsk_repeat_node_draw_pad (node, cr, data);
        break;

      case GSK_REPEAT_REPEAT:
        gsk_repeat_node_draw_repeat (node, cr, data);
        break;

      case GSK_REPEAT_REFLECT:
        gsk_repeat_node_draw_reflect (node, cr, data);
        break;

      default:
        g_assert_not_reached ();
        break;
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
      gsk_rect_equal (&self1->child_bounds, &self2->child_bounds) &&
      self1->repeat == self2->repeat)
    {
      cairo_region_t *sub;
      cairo_rectangle_int_t clip_rect;

      sub = cairo_region_create();
      gsk_render_node_diff (self1->child, self2->child, &(GskDiffData) { sub, data->copies, data->surface });
      gsk_rect_to_cairo_grow (&self1->child_bounds, &clip_rect);
      cairo_region_intersect_rectangle (sub, &clip_rect);
      if (cairo_region_is_empty (sub))
        {
          cairo_region_destroy (sub);
          return;
        }
      cairo_region_destroy (sub);
    }

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode **
gsk_repeat_node_get_children (GskRenderNode *node,
                              gsize         *n_children)
{
  GskRepeatNode *self = (GskRepeatNode *) node;

  *n_children = 1;
  
  return &self->child;
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
    result = gsk_repeat_node_new2 (&node->bounds, child, &self->child_bounds, self->repeat);

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
  node_class->get_children = gsk_repeat_node_get_children;
  node_class->replay = gsk_repeat_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskRepeatNode, gsk_repeat_node)

GskRenderNode *
gsk_repeat_node_new2 (const graphene_rect_t  *bounds,
                      GskRenderNode          *child,
                      const graphene_rect_t  *child_bounds,
                      GskRepeat               repeat)
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
  self->repeat = repeat;

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
  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  return gsk_repeat_node_new2 (bounds, child, child_bounds, GSK_REPEAT_REPEAT);
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

GskRepeat
gsk_repeat_node_get_repeat (GskRenderNode *node)
{
  const GskRepeatNode *self = (const GskRepeatNode *) node;

  return self->repeat;
}

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

#include "gskcrossfadenode.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrenderreplay.h"
#include "gdk/gdkcairoprivate.h"
#include "gskopacitynode.h"

#include "gskrendernodeprivate.h"

/**
 * GskCrossFadeNode:
 *
 * A render node cross fading between two child nodes.
 */
struct _GskCrossFadeNode
{
  GskRenderNode render_node;

  GskRenderNode *start;
  GskRenderNode *end;
  float          progress;
};

static void
gsk_cross_fade_node_finalize (GskRenderNode *node)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CROSS_FADE_NODE));

  gsk_render_node_unref (self->start);
  gsk_render_node_unref (self->end);

  parent_class->finalize (node);
}

static void
gsk_cross_fade_node_draw (GskRenderNode *node,
                          cairo_t       *cr,
                          GdkColorState *ccs)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;

  if (gdk_cairo_is_all_clipped (cr))
    return;

  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
  gsk_render_node_draw_ccs (self->start, cr, ccs);

  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
  gsk_render_node_draw_ccs (self->end, cr, ccs);

  cairo_pop_group_to_source (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint_with_alpha (cr, self->progress);

  cairo_pop_group_to_source (cr); /* resets operator */
  cairo_paint (cr);
}

static void
gsk_cross_fade_node_diff (GskRenderNode *node1,
                          GskRenderNode *node2,
                          GskDiffData   *data)
{
  GskCrossFadeNode *self1 = (GskCrossFadeNode *) node1;
  GskCrossFadeNode *self2 = (GskCrossFadeNode *) node2;

  if (self1->progress == self2->progress)
    {
      gsk_render_node_diff (self1->start, self2->start, data);
      gsk_render_node_diff (self1->end, self2->end, data);
      return;
    }

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode *
gsk_cross_fade_node_replay (GskRenderNode   *node,
                            GskRenderReplay *replay)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;
  GskRenderNode *result, *start, *end;

  start = gsk_render_replay_filter_node (replay, self->start);
  end = gsk_render_replay_filter_node (replay, self->end);

  if (start == NULL)
    {
      if (end == NULL)
        return NULL;

      result = gsk_opacity_node_new (end, self->progress);
    }
  else if (end == NULL)
    {
      result = gsk_opacity_node_new (start, 1.0 - self->progress);
    }
  else if (start == self->start && end == self->end)
    {
      result = gsk_render_node_ref (node);
    }
  else
    {
      result = gsk_cross_fade_node_new (start, end, self->progress);
    }

  g_clear_pointer (&start, gsk_render_node_unref);
  g_clear_pointer (&end, gsk_render_node_unref);

  return result;
}

static gboolean
gsk_cross_fade_node_get_opaque_rect (GskRenderNode   *node,
                                     graphene_rect_t *opaque)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;
  graphene_rect_t start_opaque, end_opaque;

  if (!gsk_render_node_get_opaque_rect (self->start, &start_opaque) ||
      !gsk_render_node_get_opaque_rect (self->end, &end_opaque))
    return FALSE;

  return graphene_rect_intersection (&start_opaque, &end_opaque, opaque);
}

static void
gsk_cross_fade_node_class_init (gpointer g_class,
                                gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_CROSS_FADE_NODE;

  node_class->finalize = gsk_cross_fade_node_finalize;
  node_class->draw = gsk_cross_fade_node_draw;
  node_class->diff = gsk_cross_fade_node_diff;
  node_class->replay = gsk_cross_fade_node_replay;
  node_class->get_opaque_rect = gsk_cross_fade_node_get_opaque_rect;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskCrossFadeNode, gsk_cross_fade_node)

/**
 * gsk_cross_fade_node_new:
 * @start: The start node to be drawn
 * @end: The node to be cross_fadeed onto the @start node
 * @progress: How far the fade has progressed from start to end. The value will
 *     be clamped to the range [0 ... 1]
 *
 * Creates a `GskRenderNode` that will do a cross-fade between @start and @end.
 *
 * Returns: (transfer full) (type GskCrossFadeNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_cross_fade_node_new (GskRenderNode *start,
                         GskRenderNode *end,
                         float          progress)
{
  GskCrossFadeNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (start), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (end), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_CROSS_FADE_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = start->fully_opaque && end->fully_opaque &&
    gsk_rect_equal (&start->bounds, &end->bounds);

  self->start = gsk_render_node_ref (start);
  self->end = gsk_render_node_ref (end);
  self->progress = CLAMP (progress, 0.0, 1.0);

  graphene_rect_union (&start->bounds, &end->bounds, &node->bounds);

  node->preferred_depth = gdk_memory_depth_merge (gsk_render_node_get_preferred_depth (start),
                                                  gsk_render_node_get_preferred_depth (end));
  node->is_hdr = gsk_render_node_is_hdr (start) ||
                 gsk_render_node_is_hdr (end);

  return node;
}

/**
 * gsk_cross_fade_node_get_start_child:
 * @node: (type GskCrossFadeNode): a cross-fading `GskRenderNode`
 *
 * Retrieves the child `GskRenderNode` at the beginning of the cross-fade.
 *
 * Returns: (transfer none): a `GskRenderNode`
 */
GskRenderNode *
gsk_cross_fade_node_get_start_child (const GskRenderNode *node)
{
  const GskCrossFadeNode *self = (const GskCrossFadeNode *) node;

  return self->start;
}

/**
 * gsk_cross_fade_node_get_end_child:
 * @node: (type GskCrossFadeNode): a cross-fading `GskRenderNode`
 *
 * Retrieves the child `GskRenderNode` at the end of the cross-fade.
 *
 * Returns: (transfer none): a `GskRenderNode`
 */
GskRenderNode *
gsk_cross_fade_node_get_end_child (const GskRenderNode *node)
{
  const GskCrossFadeNode *self = (const GskCrossFadeNode *) node;

  return self->end;
}

/**
 * gsk_cross_fade_node_get_progress:
 * @node: (type GskCrossFadeNode): a cross-fading `GskRenderNode`
 *
 * Retrieves the progress value of the cross fade.
 *
 * Returns: the progress value, between 0 and 1
 */
float
gsk_cross_fade_node_get_progress (const GskRenderNode *node)
{
  const GskCrossFadeNode *self = (const GskCrossFadeNode *) node;

  return self->progress;
}

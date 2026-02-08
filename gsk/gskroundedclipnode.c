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

#include "gskroundedclipnode.h"

#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

#include "gskrectprivate.h"
#include "gskroundedrectprivate.h"

/**
 * GskRoundedClipNode:
 *
 * A render node applying a rounded rectangle clip to its single child.
 */
struct _GskRoundedClipNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GskRoundedRect clip;
};

static void
gsk_rounded_clip_node_finalize (GskRenderNode *node)
{
  GskRoundedClipNode *self = (GskRoundedClipNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_ROUNDED_CLIP_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
gsk_rounded_clip_node_draw (GskRenderNode *node,
                            cairo_t       *cr,
                            GskCairoData  *data)
{
  GskRoundedClipNode *self = (GskRoundedClipNode *) node;

  cairo_save (cr);

  gsk_rounded_rect_path (&self->clip, cr);
  cairo_clip (cr);

  gsk_render_node_draw_full (self->child, cr, data);

  cairo_restore (cr);
}

static void
gsk_rounded_clip_node_diff (GskRenderNode *node1,
                            GskRenderNode *node2,
                            GskDiffData   *data)
{
  GskRoundedClipNode *self1 = (GskRoundedClipNode *) node1;
  GskRoundedClipNode *self2 = (GskRoundedClipNode *) node2;

  if (gsk_rounded_rect_equal (&self1->clip, &self2->clip))
    {
      cairo_region_t *save;
      cairo_rectangle_int_t clip_rect;

      save = cairo_region_copy (data->region);
      gsk_render_node_diff (self1->child, self2->child, data);
      gsk_rect_to_cairo_grow (&self1->clip.bounds, &clip_rect);
      cairo_region_intersect_rectangle (data->region, &clip_rect);
      cairo_region_union (data->region, save);
      cairo_region_destroy (save);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static void
gsk_rounded_clip_node_render_opacity (GskRenderNode  *node,
                                      GskOpacityData *data)
{
  GskRoundedClipNode *self = (GskRoundedClipNode *) node;
  GskOpacityData child_data;
  graphene_rect_t opaque, wide_opaque, high_opaque;
  double start, end;

  child_data = GSK_OPACITY_DATA_INIT_COPY (data);
  gsk_render_node_render_opacity (self->child, &child_data);

  if (gsk_render_node_clears_background (self->child) &&
      !gsk_rect_contains_rect (&child_data.opaque, &self->clip.bounds))
    {
      if (!gsk_rect_subtract (&data->opaque, &self->clip.bounds, &data->opaque))
        data->opaque = GRAPHENE_RECT_INIT (0, 0, 0, 0);
    }

  if (!gsk_rect_is_empty (&child_data.opaque))
    {
      wide_opaque = self->clip.bounds;
      start = MAX(self->clip.corner[GSK_CORNER_TOP_LEFT].height, self->clip.corner[GSK_CORNER_TOP_RIGHT].height);
      end = MAX(self->clip.corner[GSK_CORNER_BOTTOM_LEFT].height, self->clip.corner[GSK_CORNER_BOTTOM_RIGHT].height);
      wide_opaque.size.height -= MIN (wide_opaque.size.height, start + end);
      wide_opaque.origin.y += start;
      graphene_rect_intersection (&wide_opaque, &child_data.opaque, &wide_opaque);

      high_opaque = self->clip.bounds;
      start = MAX(self->clip.corner[GSK_CORNER_TOP_LEFT].width, self->clip.corner[GSK_CORNER_BOTTOM_LEFT].width);
      end = MAX(self->clip.corner[GSK_CORNER_TOP_RIGHT].width, self->clip.corner[GSK_CORNER_BOTTOM_RIGHT].width);
      high_opaque.size.width -= MIN (high_opaque.size.width, start + end);
      high_opaque.origin.x += start;
      graphene_rect_intersection (&high_opaque, &child_data.opaque, &high_opaque);

      if (wide_opaque.size.width * wide_opaque.size.height > high_opaque.size.width * high_opaque.size.height)
        opaque = wide_opaque;
      else
        opaque = high_opaque;

      if (!gsk_rect_is_empty (&opaque))
        {
          if (gsk_rect_is_empty (&child_data.opaque))
            data->opaque = opaque;
          else
            gsk_rect_coverage (&data->opaque, &opaque, &data->opaque);
        }
    }
}

static GskRenderNode **
gsk_rounded_clip_node_get_children (GskRenderNode *node,
                                    gsize         *n_children)
{
  GskRoundedClipNode *self = (GskRoundedClipNode *) node;

  *n_children = 1;
  
  return &self->child;
}

static GskRenderNode *
gsk_rounded_clip_node_replay (GskRenderNode   *node,
                              GskRenderReplay *replay)
{
  GskRoundedClipNode *self = (GskRoundedClipNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_rounded_clip_node_new (child, &self->clip);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_rounded_clip_node_class_init (gpointer g_class,
                                  gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_ROUNDED_CLIP_NODE;

  node_class->finalize = gsk_rounded_clip_node_finalize;
  node_class->draw = gsk_rounded_clip_node_draw;
  node_class->diff = gsk_rounded_clip_node_diff;
  node_class->get_children = gsk_rounded_clip_node_get_children;
  node_class->replay = gsk_rounded_clip_node_replay;
  node_class->render_opacity = gsk_rounded_clip_node_render_opacity;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskRoundedClipNode, gsk_rounded_clip_node)

/**
 * gsk_rounded_clip_node_new:
 * @child: The node to draw
 * @clip: The clip to apply
 *
 * Creates a `GskRenderNode` that will clip the @child to the area
 * given by @clip.
 *
 * Returns: (transfer full) (type GskRoundedClipNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_rounded_clip_node_new (GskRenderNode         *child,
                           const GskRoundedRect  *clip)
{
  GskRoundedClipNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (clip != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_ROUNDED_CLIP_NODE);
  node = (GskRenderNode *) self;

  self->child = gsk_render_node_ref (child);
  gsk_rounded_rect_init_copy (&self->clip, clip);

  if (!gsk_rect_intersection (&self->clip.bounds, &child->bounds, &node->bounds))
    node->bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->clears_background = gsk_render_node_clears_background (child);
  node->copy_mode = gsk_render_node_get_copy_mode (child) ? GSK_COPY_ANY : GSK_COPY_NONE;
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);
  node->needs_blending = gsk_render_node_needs_blending (child);

  return node;
}

/**
 * gsk_rounded_clip_node_get_child:
 * @node: (type GskRoundedClipNode): a rounded clip `GskRenderNode`
 *
 * Gets the child node that is getting clipped by the given @node.
 *
 * Returns: (transfer none): The child that is getting clipped
 **/
GskRenderNode *
gsk_rounded_clip_node_get_child (const GskRenderNode *node)
{
  const GskRoundedClipNode *self = (const GskRoundedClipNode *) node;

  return self->child;
}

/**
 * gsk_rounded_clip_node_get_clip:
 * @node: (type GskRoundedClipNode): a rounded clip `GskRenderNode`
 *
 * Retrieves the rounded rectangle used to clip the contents of the @node.
 *
 * Returns: (transfer none): a rounded rectangle
 */
const GskRoundedRect *
gsk_rounded_clip_node_get_clip (const GskRenderNode *node)
{
  const GskRoundedClipNode *self = (const GskRoundedClipNode *) node;

  return &self->clip;
}

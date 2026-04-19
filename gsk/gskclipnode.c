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

#include "gskclipnodeprivate.h"

#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"


#include "gdk/gdkcairoprivate.h"
#include "gskrectprivate.h"
#include "gpu/gskgpuocclusionprivate.h"

/**
 * GskClipNode:
 *
 * A render node applying a rectangular clip to its single child node.
 */
struct _GskClipNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  graphene_rect_t clip;
  GskRectSnap snap;
};

static void
gsk_clip_node_finalize (GskRenderNode *node)
{
  GskClipNode *self = (GskClipNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CLIP_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
gsk_clip_node_draw (GskRenderNode *node,
                    cairo_t       *cr,
                    GskCairoData  *data)
{
  GskClipNode *self = (GskClipNode *) node;
  graphene_rect_t clip;

  if (!gsk_cairo_rect_snap (cr, &self->clip, self->snap, &clip))
    return;

  cairo_save (cr);

  gdk_cairo_rect (cr, &clip);
  cairo_clip (cr);

  gsk_render_node_draw_full (self->child, cr, data);

  cairo_restore (cr);
}

static void
gsk_clip_node_diff (GskRenderNode *node1,
                    GskRenderNode *node2,
                    GskDiffData   *data)
{
  GskClipNode *self1 = (GskClipNode *) node1;
  GskClipNode *self2 = (GskClipNode *) node2;

  if (gsk_rect_equal (&self1->clip, &self2->clip) &&
      self1->snap == self2->snap)
    {
      cairo_region_t *save;
      cairo_rectangle_int_t clip_rect;

      save = cairo_region_copy (data->region);
      gsk_render_node_diff (self1->child, self2->child, data);
      gsk_rect_to_cairo_grow (&self1->clip, &clip_rect);
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
gsk_clip_node_render_opacity (GskRenderNode  *node,
                              GskOpacityData *data)
{
  GskClipNode *self = (GskClipNode *) node;
  GskOpacityData child_data;

  child_data = GSK_OPACITY_DATA_INIT_COPY (data);
  gsk_render_node_render_opacity (self->child, &child_data);

  if (gsk_render_node_clears_background (self->child) &&
      !gsk_rect_contains_rect (&child_data.opaque, &self->clip))
    {
      if (!gsk_rect_subtract (&data->opaque, &self->clip, &data->opaque))
        data->opaque = GRAPHENE_RECT_INIT (0, 0, 0, 0);
    }

  if (gsk_rect_intersection (&child_data.opaque, &self->clip, &child_data.opaque))
    {
      if (gsk_rect_is_empty (&data->opaque))
        data->opaque = child_data.opaque;
      else
        gsk_rect_coverage (&data->opaque, &child_data.opaque, &data->opaque);
    }
}

static GskRenderNode **
gsk_clip_node_get_children (GskRenderNode *node,
                            gsize         *n_children)
{
  GskClipNode *self = (GskClipNode *) node;

  *n_children = 1;
  
  return &self->child;
}


static GskRenderNode *
gsk_clip_node_replay (GskRenderNode   *node,
                      GskRenderReplay *replay)
{
  GskClipNode *self = (GskClipNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_clip_node_new2 (child, &self->clip, self->snap);

  gsk_render_node_unref (child);

  return result;
}

static GskGpuRenderPass *
gsk_clip_node_occlusion (GskRenderNode   *node,
                         GskGpuOcclusion *occlusion)
{
  GskClipNode *self = (GskClipNode *) node;
  cairo_rectangle_int_t storage;
  GskGpuRenderPass *result;

  if (!gsk_gpu_occlusion_push_clip (occlusion, &self->clip, self->snap, &storage))
    return NULL;

  result = gsk_gpu_occlusion_try_node (occlusion, self->child, 0);
  if (result)
    return result;

  gsk_gpu_occlusion_pop_clip (occlusion, &storage);

  return NULL;
}

static void
gsk_clip_node_class_init (gpointer g_class,
                          gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_CLIP_NODE;

  node_class->finalize = gsk_clip_node_finalize;
  node_class->draw = gsk_clip_node_draw;
  node_class->diff = gsk_clip_node_diff;
  node_class->get_children = gsk_clip_node_get_children;
  node_class->replay = gsk_clip_node_replay;
  node_class->render_opacity = gsk_clip_node_render_opacity;
  node_class->occlusion = gsk_clip_node_occlusion;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskClipNode, gsk_clip_node)

/*<private>
 * gsk_clip_node_new2:
 * @child: The node to draw
 * @clip: The clip to apply
 * @snap: how to snap the clip rectangle to the pixel grid
 *
 * Creates a `GskRenderNode` that will clip the @child to the area
 * given by @clip using the given snap value.
 *
 * Returns: (transfer full) (type GskClipNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_clip_node_new2 (GskRenderNode         *child,
                    const graphene_rect_t *clip,
                    GskRectSnap            snap)
{
  GskClipNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (clip != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_CLIP_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = !gsk_rect_snap_can_shrink (snap) && gsk_render_node_is_fully_opaque (child);

  self->child = gsk_render_node_ref (child);
  gsk_rect_init_from_rect (&self->clip, clip);
  gsk_rect_normalize (&self->clip);
  self->snap = snap;

  if (!gsk_rect_intersection (&self->clip, &child->bounds, &node->bounds))
    {
      /* keep the line/point, it's not worse than 0 0 0 0 */
    }

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
 * gsk_clip_node_new:
 * @child: The node to draw
 * @clip: The clip to apply
 *
 * Creates a `GskRenderNode` that will clip the @child to the area
 * given by @clip.
 *
 * Returns: (transfer full) (type GskClipNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_clip_node_new (GskRenderNode         *child,
                   const graphene_rect_t *clip)
{
  return gsk_clip_node_new2 (child, clip, GSK_RECT_SNAP_NONE);
}

/**
 * gsk_clip_node_get_child:
 * @node: (type GskClipNode): a clip @GskRenderNode
 *
 * Gets the child node that is getting clipped by the given @node.
 *
 * Returns: (transfer none): The child that is getting clipped
 **/
GskRenderNode *
gsk_clip_node_get_child (const GskRenderNode *node)
{
  const GskClipNode *self = (const GskClipNode *) node;

  return self->child;
}

/**
 * gsk_clip_node_get_clip:
 * @node: (type GskClipNode): a `GskClipNode`
 *
 * Retrieves the clip rectangle for @node.
 *
 * Returns: a clip rectangle
 */
const graphene_rect_t *
gsk_clip_node_get_clip (const GskRenderNode *node)
{
  const GskClipNode *self = (const GskClipNode *) node;

  return &self->clip;
}

/**
 * gsk_clip_node_get_snap:
 * @node: (type GskClipNode): a `GskClipNode`
 *
 * Retrieves the snap value for this node
 *
 * Returns: the snap value
 *
 * Since: 4.24
 **/
GskRectSnap
gsk_clip_node_get_snap (const GskRenderNode *node)
{
  const GskClipNode *self = (const GskClipNode *) node;

  return self->snap;
}


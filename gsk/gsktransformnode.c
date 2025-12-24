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

#include "gsktransformnode.h"

#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"
#include "gskrectprivate.h"
#include "gsktransformprivate.h"

#include "gdk/gdkcairoprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gdk/gdkrectangleprivate.h"


static void
region_union_region_affine (cairo_region_t       *region,
                            const cairo_region_t *sub,
                            float                 scale_x,
                            float                 scale_y,
                            float                 offset_x,
                            float                 offset_y)
{
  cairo_rectangle_int_t rect;
  int i;
  
  for (i = 0; i < cairo_region_num_rectangles (sub); i++)
    {
      cairo_region_get_rectangle (sub, i, &rect);
      gdk_rectangle_transform_affine (&rect, scale_x, scale_y, offset_x, offset_y, &rect);
      cairo_region_union_rectangle (region, &rect);
    }
}

/**
 * GskTransformNode:
 *
 * A render node applying a `GskTransform` to its single child node.
 */
struct _GskTransformNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GskTransform *transform;
};

static void
gsk_transform_node_finalize (GskRenderNode *node)
{
  GskTransformNode *self = (GskTransformNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_TRANSFORM_NODE));

  gsk_render_node_unref (self->child);
  gsk_transform_unref (self->transform);

  parent_class->finalize (node);
}

static void
gsk_transform_node_draw (GskRenderNode *node,
                         cairo_t       *cr,
                         GskCairoData  *data)
{
  GskTransformNode *self = (GskTransformNode *) node;
  float xx, yx, xy, yy, dx, dy;
  cairo_matrix_t ctm;

  if (gsk_transform_get_category (self->transform) < GSK_TRANSFORM_CATEGORY_2D)
    {
      GdkRGBA pink = { 255 / 255., 105 / 255., 180 / 255., 1.0 };
      gdk_cairo_set_source_rgba_ccs (cr, data->ccs, &pink);
      gdk_cairo_rect (cr, &node->bounds);
      cairo_fill (cr);
      return;
    }

  gsk_transform_to_2d (self->transform, &xx, &yx, &xy, &yy, &dx, &dy);
  cairo_matrix_init (&ctm, xx, yx, xy, yy, dx, dy);
  if (xx * yy == xy * yx)
    {
      /* broken matrix here. This can happen during transitions
       * (like when flipping an axis at the point where scale == 0)
       * and just means that nothing should be drawn.
       * But Cairo throws lots of ugly errors instead of silently
       * going on. So we silently go on.
       */
      return;
    }
  cairo_transform (cr, &ctm);

  gsk_render_node_draw_full (self->child, cr, data);
}

static gboolean
gsk_transform_node_can_diff (const GskRenderNode *node1,
                             const GskRenderNode *node2)
{
  GskTransformNode *self1 = (GskTransformNode *) node1;
  GskTransformNode *self2 = (GskTransformNode *) node2;

  if (!gsk_transform_equal (self1->transform, self2->transform))
    return FALSE;

  return gsk_render_node_can_diff (self1->child, self2->child);
}

static void
gsk_transform_node_diff (GskRenderNode *node1,
                         GskRenderNode *node2,
                         GskDiffData   *data)
{
  GskTransformNode *self1 = (GskTransformNode *) node1;
  GskTransformNode *self2 = (GskTransformNode *) node2;

  if (!gsk_transform_equal (self1->transform, self2->transform))
    {
      gsk_render_node_diff_impossible (node1, node2, data);
      return;
    }

  if (self1->child == self2->child)
    return;

  switch (gsk_transform_get_category (self1->transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      gsk_render_node_diff (self1->child, self2->child, data);
      break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;
        gsk_transform_to_translate (self1->transform, &dx, &dy);
        if (floorf (dx) == dx && floorf (dy) != dy)
          {
            cairo_region_translate (data->region, -dx, -dy);
            gsk_render_node_diff (self1->child, self2->child, data);
            cairo_region_translate (data->region, dx, dy);
            break;
          }
      }
      G_GNUC_FALLTHROUGH;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        cairo_region_t *sub;
        float scale_x, scale_y, dx, dy;
        gsk_transform_to_affine (self1->transform, &scale_x, &scale_y, &dx, &dy);
        sub = cairo_region_create ();
        if (gsk_render_node_get_copy_mode (node1) != GSK_COPY_NONE ||
            gsk_render_node_get_copy_mode (node2) != GSK_COPY_NONE)
          region_union_region_affine (sub, data->region, 1.0f / scale_x, 1.0f / scale_y, - dx / scale_x, - dy / scale_y);
        gsk_render_node_diff (self1->child, self2->child, &(GskDiffData) { sub, data->copies, data->surface });
        region_union_region_affine (data->region, sub, scale_x, scale_y, dx, dy);
        cairo_region_destroy (sub);
      }
      break;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
    default:
      gsk_render_node_diff_impossible (node1, node2, data);
      break;
    }
}

static GskRenderNode **
gsk_transform_node_get_children (GskRenderNode *node,
                                 gsize         *n_children)
{
  GskTransformNode *self = (GskTransformNode *) node;

  *n_children = 1;
  
  return &self->child;
}

static GskRenderNode *
gsk_transform_node_replay (GskRenderNode   *node,
                           GskRenderReplay *replay)
{
  GskTransformNode *self = (GskTransformNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_transform_node_new (child, self->transform);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_transform_node_render_opacity (GskRenderNode  *node,
                                   GskOpacityData *data)
{
  GskTransformNode *self = (GskTransformNode *) node;

  if (gsk_transform_get_fine_category (self->transform) < GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL)
    {
      /* too complex, skip child */
      if (gsk_render_node_clears_background (node) && !gsk_rect_is_empty (&data->opaque))
        {
          if (!gsk_rect_subtract (&data->opaque, &node->bounds, &data->opaque))
            data->opaque = GRAPHENE_RECT_INIT (0, 0, 0, 0);
        }
      return;
    }

  if (!gsk_rect_is_empty (&data->opaque))
    {
      GskTransform *inverse;

      inverse = gsk_transform_invert (gsk_transform_ref (self->transform));
      if (inverse == NULL)
        return;

      gsk_transform_transform_bounds (inverse, &data->opaque, &data->opaque);
      gsk_transform_unref (inverse);
    }


  gsk_render_node_render_opacity (self->child, data);

  if (!gsk_rect_is_empty (&data->opaque))
    gsk_transform_transform_bounds (self->transform, &data->opaque, &data->opaque);
}

static void
gsk_transform_node_class_init (gpointer g_class,
                               gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_TRANSFORM_NODE;

  node_class->finalize = gsk_transform_node_finalize;
  node_class->draw = gsk_transform_node_draw;
  node_class->can_diff = gsk_transform_node_can_diff;
  node_class->diff = gsk_transform_node_diff;
  node_class->get_children = gsk_transform_node_get_children;
  node_class->replay = gsk_transform_node_replay;
  node_class->render_opacity = gsk_transform_node_render_opacity;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskTransformNode, gsk_transform_node)

/**
 * gsk_transform_node_new:
 * @child: The node to transform
 * @transform: (transfer none) (nullable): The transform to apply
 *
 * Creates a `GskRenderNode` that will transform the given @child
 * with the given @transform.
 *
 * Returns: (transfer full) (type GskTransformNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_transform_node_new (GskRenderNode *child,
                        GskTransform  *transform)
{
  GskTransformNode *self;
  GskRenderNode *node;
  GskTransformCategory category;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  category = gsk_transform_get_category (transform);

  self = gsk_render_node_alloc (GSK_TYPE_TRANSFORM_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = child->fully_opaque && category >= GSK_TRANSFORM_CATEGORY_2D_AFFINE;

  self->child = gsk_render_node_ref (child);
  self->transform = gsk_transform_ref (transform);

  gsk_transform_transform_bounds (self->transform,
                                  &child->bounds,
                                  &node->bounds);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->clears_background = gsk_render_node_clears_background (child);
  node->copy_mode = gsk_render_node_get_copy_mode (child) ? GSK_COPY_ANY : GSK_COPY_NONE;
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);

  return node;
}

/**
 * gsk_transform_node_get_child:
 * @node: (type GskTransformNode): a `GskRenderNode` for a transform
 *
 * Gets the child node that is getting transformed by the given @node.
 *
 * Returns: (transfer none): The child that is getting transformed
 */
GskRenderNode *
gsk_transform_node_get_child (const GskRenderNode *node)
{
  const GskTransformNode *self = (const GskTransformNode *) node;

  return self->child;
}

/**
 * gsk_transform_node_get_transform:
 * @node: (type GskTransformNode): a `GskRenderNode` for a transform
 *
 * Retrieves the `GskTransform` used by the @node.
 *
 * Returns: (transfer none): a `GskTransform`
 */
GskTransform *
gsk_transform_node_get_transform (const GskRenderNode *node)
{
  const GskTransformNode *self = (const GskTransformNode *) node;

  return self->transform;
}

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

#include "gskinsetshadownodeprivate.h"

#include "gskrendernodeprivate.h"

#include "gskrectprivate.h"
#include "gskroundedrectprivate.h"
#include "gskcairoblurprivate.h"
#include "gskcairoshadowprivate.h"

/**
 * GskInsetShadowNode:
 *
 * A render node for an inset shadow.
 */
struct _GskInsetShadowNode
{
  GskRenderNode render_node;

  GskRoundedRect outline;
  GdkColor color;
  graphene_point_t offset;
  float spread;
  float blur_radius;
};

static void
gsk_inset_shadow_node_finalize (GskRenderNode *node)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_INSET_SHADOW_NODE));

  gdk_color_finish (&self->color);

  parent_class->finalize (node);
}

static void
gsk_inset_shadow_node_draw (GskRenderNode *node,
                            cairo_t       *cr,
                            GskCairoData  *data)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;
  GskRoundedRect box, clip_box;
  int clip_radius;
  graphene_rect_t clip_rect;
  double blur_radius;

  /* We don't need to draw invisible shadows */
  if (gdk_color_is_clear (&self->color))
    return;

  _graphene_rect_init_from_clip_extents (&clip_rect, cr);
  if (!gsk_rounded_rect_intersects_rect (&self->outline, &clip_rect))
    return;

  blur_radius = self->blur_radius / 2;

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius);

  cairo_save (cr);

  gsk_rounded_rect_path (&self->outline, cr);
  cairo_clip (cr);

  gsk_rounded_rect_init_copy (&box, &self->outline);
  gsk_rounded_rect_offset (&box, self->offset.x, self->offset.y);
  gsk_rounded_rect_shrink (&box, self->spread, self->spread, self->spread, self->spread);

  gsk_rounded_rect_init_copy (&clip_box, &self->outline);
  gsk_rounded_rect_shrink (&clip_box, -clip_radius, -clip_radius, -clip_radius, -clip_radius);

  if (!gsk_cairo_shadow_needs_blur (blur_radius))
    {
      gsk_cairo_shadow_draw (cr, data->ccs, TRUE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
    }
  else
    {
      cairo_region_t *remaining;
      cairo_rectangle_int_t r;
      int i;

      /* For the blurred case we divide the rendering into 9 parts,
       * 4 of the corners, 4 for the horizonat/vertical lines and
       * one for the interior. We make the non-interior parts
       * large enough to fit the full radius of the blur, so that
       * the interior part can be drawn solidly.
       */

      /* In the inset case we want to paint the whole clip-box.
       * We could remove the part of "box" where the blur doesn't
       * reach, but computing that is a bit tricky since the
       * rounded corners are on the "inside" of it. */
      gsk_rect_to_cairo_grow (&clip_box.bounds, &r);
      remaining = cairo_region_create_rectangle (&r);

      /* First do the corners of box */
      for (i = 0; i < 4; i++)
        {
          cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
          gsk_cairo_shadow_draw_corner (cr, data->ccs, TRUE, &box, &clip_box, blur_radius, &self->color, i, &r);
          cairo_restore (cr);

          /* We drew the region, remove it from remaining */
          cairo_region_subtract_rectangle (remaining, &r);
        }

      /* Then the sides */
      for (i = 0; i < 4; i++)
        {
          cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
          gsk_cairo_shadow_draw_side (cr, data->ccs, TRUE, &box, &clip_box, blur_radius, &self->color, i, &r);
          cairo_restore (cr);

          /* We drew the region, remove it from remaining */
          cairo_region_subtract_rectangle (remaining, &r);
        }

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      gsk_cairo_shadow_draw (cr, data->ccs, TRUE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
      cairo_restore (cr);

      cairo_region_destroy (remaining);
    }

  cairo_restore (cr);
}

static void
gsk_inset_shadow_node_diff (GskRenderNode *node1,
                            GskRenderNode *node2,
                            GskDiffData   *data)
{
  GskInsetShadowNode *self1 = (GskInsetShadowNode *) node1;
  GskInsetShadowNode *self2 = (GskInsetShadowNode *) node2;

  if (gsk_rounded_rect_equal (&self1->outline, &self2->outline) &&
      gdk_color_equal (&self1->color, &self2->color) &&
      graphene_point_equal (&self1->offset, &self2->offset) &&
      self1->spread == self2->spread &&
      self1->blur_radius == self2->blur_radius)
    return;

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode *
gsk_render_node_replay_as_self (GskRenderNode   *node,
                                GskRenderReplay *replay)
{
  return gsk_render_node_ref (node);
}

static void
gsk_inset_shadow_node_class_init (gpointer g_class,
                                  gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_INSET_SHADOW_NODE;

  node_class->finalize = gsk_inset_shadow_node_finalize;
  node_class->draw = gsk_inset_shadow_node_draw;
  node_class->diff = gsk_inset_shadow_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskInsetShadowNode, gsk_inset_shadow_node)

/**
 * gsk_inset_shadow_node_new:
 * @outline: outline of the region containing the shadow
 * @color: color of the shadow
 * @dx: horizontal offset of shadow
 * @dy: vertical offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a `GskRenderNode` that will render an inset shadow
 * into the box given by @outline.
 *
 * Returns: (transfer full) (type GskInsetShadowNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_inset_shadow_node_new (const GskRoundedRect *outline,
                           const GdkRGBA        *color,
                           float                 dx,
                           float                 dy,
                           float                 spread,
                           float                 blur_radius)
{
  GdkColor color2;
  GskRenderNode *node;

  gdk_color_init_from_rgba (&color2, color);
  node = gsk_inset_shadow_node_new2 (outline,
                                     &color2,
                                     &GRAPHENE_POINT_INIT (dx, dy),
                                     spread, blur_radius);
  gdk_color_finish (&color2);

  return node;
}

/*< private >
 * gsk_inset_shadow_node_new2:
 * @outline: outline of the region containing the shadow
 * @color: color of the shadow
 * @offset: offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a `GskRenderNode` that will render an inset shadow
 * into the box given by @outline.
 *
 * Returns: (transfer full) (type GskInsetShadowNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_inset_shadow_node_new2 (const GskRoundedRect   *outline,
                            const GdkColor         *color,
                            const graphene_point_t *offset,
                            float                   spread,
                            float                   blur_radius)
{
  GskInsetShadowNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (color != NULL, NULL);
  g_return_val_if_fail (offset != NULL, NULL);
  g_return_val_if_fail (blur_radius >= 0, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_INSET_SHADOW_NODE);
  node = (GskRenderNode *) self;
  node->preferred_depth = GDK_MEMORY_NONE;

  gsk_rounded_rect_init_copy (&self->outline, outline);
  gdk_color_init_copy (&self->color, color);
  self->offset = *offset;
  self->spread = spread;
  self->blur_radius = blur_radius;

  gsk_rect_init_from_rect (&node->bounds, &self->outline.bounds);

  return node;
}

/**
 * gsk_inset_shadow_node_get_outline:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the outline rectangle of the inset shadow.
 *
 * Returns: (transfer none): a rounded rectangle
 */
const GskRoundedRect *
gsk_inset_shadow_node_get_outline (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return &self->outline;
}

/**
 * gsk_inset_shadow_node_get_color:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the color of the inset shadow.
 *
 * The value returned by this function will not be correct
 * if the render node was created for a non-sRGB color.
 *
 * Returns: (transfer none): the color of the shadow
 */
const GdkRGBA *
gsk_inset_shadow_node_get_color (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  /* NOTE: This is only correct for nodes with sRGB colors */
  return (const GdkRGBA *) &self->color.values;
}

/*< private >
 * gsk_inset_shadow_node_get_gdk_color:
 * @node: (type GskInsetShadowNode): a `GskRenderNode`
 *
 * Retrieves the color of the given @node.
 *
 * Returns: (transfer none): the color of the node
 */
const GdkColor *
gsk_inset_shadow_node_get_gdk_color (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return &self->color;
}

/**
 * gsk_inset_shadow_node_get_dx:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the horizontal offset of the inset shadow.
 *
 * Returns: an offset, in pixels
 */
float
gsk_inset_shadow_node_get_dx (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return self->offset.x;
}

/**
 * gsk_inset_shadow_node_get_dy:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the vertical offset of the inset shadow.
 *
 * Returns: an offset, in pixels
 */
float
gsk_inset_shadow_node_get_dy (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return self->offset.y;
}

/**
 * gsk_inset_shadow_node_get_offset:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the offset of the inset shadow.
 *
 * Returns: an offset, in pixels
 **/
const graphene_point_t *
gsk_inset_shadow_node_get_offset (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return &self->offset;
}

/**
 * gsk_inset_shadow_node_get_spread:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves how much the shadow spreads inwards.
 *
 * Returns: the size of the shadow, in pixels
 */
float
gsk_inset_shadow_node_get_spread (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return self->spread;
}

/**
 * gsk_inset_shadow_node_get_blur_radius:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the blur radius to apply to the shadow.
 *
 * Returns: the blur radius, in pixels
 */
float
gsk_inset_shadow_node_get_blur_radius (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return self->blur_radius;
}

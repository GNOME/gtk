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

#include "gskoutsetshadownodeprivate.h"

#include "gskrendernodeprivate.h"

#include "gskroundedrectprivate.h"
#include "gskrectprivate.h"
#include "gskcairoblurprivate.h"
#include "gskcairoshadowprivate.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskOutsetShadowNode:
 *
 * A render node for an outset shadow.
 */
struct _GskOutsetShadowNode
{
  GskRenderNode render_node;

  GskRoundedRect outline;
  GdkColor color;
  graphene_point_t offset;
  float spread;
  float blur_radius;
};

static void
gsk_outset_shadow_node_finalize (GskRenderNode *node)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_OUTSET_SHADOW_NODE));

  gdk_color_finish (&self->color);

  parent_class->finalize (node);
}

static void
gsk_outset_shadow_get_extents (GskOutsetShadowNode *self,
                               float               *top,
                               float               *right,
                               float               *bottom,
                               float               *left)
{
  float clip_radius;

  clip_radius = gsk_cairo_blur_compute_pixels (ceil (self->blur_radius / 2.0));
  *top = MAX (0, ceil (clip_radius + self->spread - self->offset.y));
  *right = MAX (0, ceil (clip_radius + self->spread + self->offset.x));
  *bottom = MAX (0, ceil (clip_radius + self->spread + self->offset.y));
  *left = MAX (0, ceil (clip_radius + self->spread - self->offset.x));
}

static void
gsk_outset_shadow_node_draw (GskRenderNode *node,
                             cairo_t       *cr,
                             GskCairoData  *data)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;
  GskRoundedRect box, clip_box;
  int clip_radius;
  graphene_rect_t clip_rect;
  float top, right, bottom, left;
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

  gsk_rounded_rect_init_copy (&clip_box, &self->outline);
  gsk_outset_shadow_get_extents (self, &top, &right, &bottom, &left);
  gsk_rounded_rect_shrink (&clip_box, -top, -right, -bottom, -left);

  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
  gsk_rounded_rect_path (&self->outline, cr);
  gdk_cairo_rect (cr, &clip_box.bounds);

  cairo_clip (cr);

  gsk_rounded_rect_init_copy (&box, &self->outline);
  gsk_rounded_rect_offset (&box, self->offset.x, self->offset.y);
  gsk_rounded_rect_shrink (&box, -self->spread, -self->spread, -self->spread, -self->spread);

  if (!gsk_cairo_shadow_needs_blur (blur_radius))
    {
      gsk_cairo_shadow_draw (cr, data->ccs, FALSE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
    }
  else
    {
      int i;
      cairo_region_t *remaining;
      cairo_rectangle_int_t r;

      /* For the blurred case we divide the rendering into 9 parts,
       * 4 of the corners, 4 for the horizonat/vertical lines and
       * one for the interior. We make the non-interior parts
       * large enough to fit the full radius of the blur, so that
       * the interior part can be drawn solidly.
       */

      /* In the outset case we want to paint the entire box, plus as far
       * as the radius reaches from it */
      r.x = floor (box.bounds.origin.x - clip_radius);
      r.y = floor (box.bounds.origin.y - clip_radius);
      r.width = ceil (box.bounds.origin.x + box.bounds.size.width + clip_radius) - r.x;
      r.height = ceil (box.bounds.origin.y + box.bounds.size.height + clip_radius) - r.y;

      remaining = cairo_region_create_rectangle (&r);

      /* First do the corners of box */
      for (i = 0; i < 4; i++)
        {
          cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
          gsk_cairo_shadow_draw_corner (cr, data->ccs, FALSE, &box, &clip_box, blur_radius, &self->color, i, &r);
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
          gsk_cairo_shadow_draw_side (cr, data->ccs, FALSE, &box, &clip_box, blur_radius, &self->color, i, &r);
          cairo_restore (cr);

          /* We drew the region, remove it from remaining */
          cairo_region_subtract_rectangle (remaining, &r);
        }

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      gsk_cairo_shadow_draw (cr, data->ccs, FALSE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
      cairo_restore (cr);

      cairo_region_destroy (remaining);
    }

  cairo_restore (cr);
}

static void
gsk_outset_shadow_node_diff (GskRenderNode *node1,
                             GskRenderNode *node2,
                             GskDiffData   *data)
{
  GskOutsetShadowNode *self1 = (GskOutsetShadowNode *) node1;
  GskOutsetShadowNode *self2 = (GskOutsetShadowNode *) node2;

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
gsk_outset_shadow_node_class_init (gpointer g_class,
                                   gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_OUTSET_SHADOW_NODE;

  node_class->finalize = gsk_outset_shadow_node_finalize;
  node_class->draw = gsk_outset_shadow_node_draw;
  node_class->diff = gsk_outset_shadow_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskOutsetShadowNode, gsk_outset_shadow_node)

/**
 * gsk_outset_shadow_node_new:
 * @outline: outline of the region surrounded by shadow
 * @color: color of the shadow
 * @dx: horizontal offset of shadow
 * @dy: vertical offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a `GskRenderNode` that will render an outset shadow
 * around the box given by @outline.
 *
 * Returns: (transfer full) (type GskOutsetShadowNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_outset_shadow_node_new (const GskRoundedRect *outline,
                            const GdkRGBA        *color,
                            float                 dx,
                            float                 dy,
                            float                 spread,
                            float                 blur_radius)
{
  GdkColor color2;
  GskRenderNode *node;

  gdk_color_init_from_rgba (&color2, color);
  node = gsk_outset_shadow_node_new2 (outline,
                                      &color2,
                                      &GRAPHENE_POINT_INIT (dx, dy),
                                      spread, blur_radius);
  gdk_color_finish (&color2);

  return node;
}

/*< private >
 * gsk_outset_shadow_node_new2:
 * @outline: outline of the region surrounded by shadow
 * @color: color of the shadow
 * @offset: offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a `GskRenderNode` that will render an outset shadow
 * around the box given by @outline.
 *
 * Returns: (transfer full) (type GskOutsetShadowNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_outset_shadow_node_new2 (const GskRoundedRect   *outline,
                             const GdkColor         *color,
                             const graphene_point_t *offset,
                             float                   spread,
                             float                   blur_radius)
{
  GskOutsetShadowNode *self;
  GskRenderNode *node;
  float top, right, bottom, left;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (color != NULL, NULL);
  g_return_val_if_fail (blur_radius >= 0, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_OUTSET_SHADOW_NODE);
  node = (GskRenderNode *) self;
  node->preferred_depth = GDK_MEMORY_NONE;

  gsk_rounded_rect_init_copy (&self->outline, outline);
  gdk_color_init_copy (&self->color, color);
  self->offset = *offset;
  self->spread = spread;
  self->blur_radius = blur_radius;

  gsk_outset_shadow_get_extents (self, &top, &right, &bottom, &left);

  gsk_rect_init_from_rect (&node->bounds, &self->outline.bounds);
  node->bounds.origin.x -= left;
  node->bounds.origin.y -= top;
  node->bounds.size.width += left + right;
  node->bounds.size.height += top + bottom;

  return node;
}

/**
 * gsk_outset_shadow_node_get_outline:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the outline rectangle of the outset shadow.
 *
 * Returns: (transfer none): a rounded rectangle
 */
const GskRoundedRect *
gsk_outset_shadow_node_get_outline (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return &self->outline;
}

/**
 * gsk_outset_shadow_node_get_color:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the color of the outset shadow.
 *
 * The value returned by this function will not be correct
 * if the render node was created for a non-sRGB color.
 *
 * Returns: (transfer none): a color
 */
const GdkRGBA *
gsk_outset_shadow_node_get_color (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  /* NOTE: This is only correct for nodes with sRGB colors */
  return (const GdkRGBA *) &self->color.values;
}

/*< private >
 * gsk_outset_shadow_node_get_gdk_color:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode`
 *
 * Retrieves the color of the given @node.
 *
 * Returns: (transfer none): the color of the node
 */
const GdkColor *
gsk_outset_shadow_node_get_gdk_color (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return &self->color;
}

/**
 * gsk_outset_shadow_node_get_dx:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the horizontal offset of the outset shadow.
 *
 * Returns: an offset, in pixels
 */
float
gsk_outset_shadow_node_get_dx (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return self->offset.x;
}

/**
 * gsk_outset_shadow_node_get_dy:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the vertical offset of the outset shadow.
 *
 * Returns: an offset, in pixels
 */
float
gsk_outset_shadow_node_get_dy (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return self->offset.y;
}

/**
 * gsk_outset_shadow_node_get_offset:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the offset of the outset shadow.
 *
 * Returns: an offset, in pixels
 **/
const graphene_point_t *
gsk_outset_shadow_node_get_offset (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return &self->offset;
}

/**
 * gsk_outset_shadow_node_get_spread:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves how much the shadow spreads outwards.
 *
 * Returns: the size of the shadow, in pixels
 */
float
gsk_outset_shadow_node_get_spread (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return self->spread;
}

/**
 * gsk_outset_shadow_node_get_blur_radius:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the blur radius of the shadow.
 *
 * Returns: the blur radius, in pixels
 */
float
gsk_outset_shadow_node_get_blur_radius (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return self->blur_radius;
}

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

#include "gskblurnode.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrenderreplay.h"

#include "gskcairoblurprivate.h"

/**
 * GskBlurNode:
 *
 * A render node applying a blur effect to its single child.
 */
struct _GskBlurNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  float radius;
};

static void
gsk_blur_node_finalize (GskRenderNode *node)
{
  GskBlurNode *self = (GskBlurNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_BLUR_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
blur_once (cairo_surface_t *src,
           cairo_surface_t *dest,
           int radius,
           guchar *div_kernel_size)
{
  int width, height, src_rowstride, dest_rowstride, n_channels;
  guchar *p_src, *p_dest, *c1, *c2;
  int x, y, i, i1, i2, width_minus_1, height_minus_1, radius_plus_1;
  int r, g, b, a;
  guchar *p_dest_row, *p_dest_col;

  width = cairo_image_surface_get_width (src);
  height = cairo_image_surface_get_height (src);
  n_channels = 4;
  radius_plus_1 = radius + 1;

  /* horizontal blur */
  p_src = cairo_image_surface_get_data (src);
  p_dest = cairo_image_surface_get_data (dest);
  src_rowstride = cairo_image_surface_get_stride (src);
  dest_rowstride = cairo_image_surface_get_stride (dest);

  width_minus_1 = width - 1;
  for (y = 0; y < height; y++)
    {
      /* calc the initial sums of the kernel */
      r = g = b = a = 0;
      for (i = -radius; i <= radius; i++)
        {
          c1 = p_src + (CLAMP (i, 0, width_minus_1) * n_channels);
          r += c1[0];
          g += c1[1];
          b += c1[2];
          a += c1[3];
        }
      p_dest_row = p_dest;
      for (x = 0; x < width; x++)
        {
          /* set as the mean of the kernel */
          p_dest_row[0] = div_kernel_size[r];
          p_dest_row[1] = div_kernel_size[g];
          p_dest_row[2] = div_kernel_size[b];
          p_dest_row[3] = div_kernel_size[a];
          p_dest_row += n_channels;

          /* the pixel to add to the kernel */
          i1 = x + radius_plus_1;
          if (i1 > width_minus_1)
            i1 = width_minus_1;
          c1 = p_src + (i1 * n_channels);

          /* the pixel to remove from the kernel */
          i2 = x - radius;
          if (i2 < 0)
            i2 = 0;
          c2 = p_src + (i2 * n_channels);

          /* calc the new sums of the kernel */
          r += c1[0] - c2[0];
          g += c1[1] - c2[1];
          b += c1[2] - c2[2];
          a += c1[3] - c2[3];
        }

      p_src += src_rowstride;
      p_dest += dest_rowstride;
    }

  /* vertical blur */
  p_src = cairo_image_surface_get_data (dest);
  p_dest = cairo_image_surface_get_data (src);
  src_rowstride = cairo_image_surface_get_stride (dest);
  dest_rowstride = cairo_image_surface_get_stride (src);

  height_minus_1 = height - 1;
  for (x = 0; x < width; x++)
    {
      /* calc the initial sums of the kernel */
      r = g = b = a = 0;
      for (i = -radius; i <= radius; i++)
        {
          c1 = p_src + (CLAMP (i, 0, height_minus_1) * src_rowstride);
          r += c1[0];
          g += c1[1];
          b += c1[2];
          a += c1[3];
        }

      p_dest_col = p_dest;
      for (y = 0; y < height; y++)
        {
          /* set as the mean of the kernel */

          p_dest_col[0] = div_kernel_size[r];
          p_dest_col[1] = div_kernel_size[g];
          p_dest_col[2] = div_kernel_size[b];
          p_dest_col[3] = div_kernel_size[a];
          p_dest_col += dest_rowstride;

          /* the pixel to add to the kernel */
          i1 = y + radius_plus_1;
          if (i1 > height_minus_1)
            i1 = height_minus_1;
          c1 = p_src + (i1 * src_rowstride);

          /* the pixel to remove from the kernel */
          i2 = y - radius;
          if (i2 < 0)
            i2 = 0;
          c2 = p_src + (i2 * src_rowstride);
          /* calc the new sums of the kernel */
          r += c1[0] - c2[0];
          g += c1[1] - c2[1];
          b += c1[2] - c2[2];
          a += c1[3] - c2[3];
        }

      p_src += n_channels;
      p_dest += n_channels;
    }
}

static void
blur_image_surface (cairo_surface_t *surface, int radius, int iterations)
{
  int kernel_size;
  int i;
  guchar *div_kernel_size;
  cairo_surface_t *tmp;
  int width, height;

  g_assert (radius >= 0);

  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);
  tmp = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  kernel_size = 2 * radius + 1;
  div_kernel_size = g_new (guchar, 256 * kernel_size);
  for (i = 0; i < 256 * kernel_size; i++)
    div_kernel_size[i] = (guchar) (i / kernel_size);

  while (iterations-- > 0)
    blur_once (surface, tmp, radius, div_kernel_size);

  g_free (div_kernel_size);
  cairo_surface_destroy (tmp);
}

static void
gsk_blur_node_draw (GskRenderNode *node,
                    cairo_t       *cr,
                    GskCairoData  *data)
{
  GskBlurNode *self = (GskBlurNode *) node;
  cairo_surface_t *surface;
  cairo_t *cr2;
  graphene_rect_t blur_bounds;
  double clip_radius;

  clip_radius = gsk_cairo_blur_compute_pixels (0.5 * self->radius);

  /* We need to extend the clip by the blur radius
   * so we can blur pixels in that region */
  _graphene_rect_init_from_clip_extents (&blur_bounds, cr);
  graphene_rect_inset (&blur_bounds, - clip_radius, - clip_radius);
  if (!gsk_rect_intersection (&blur_bounds, &node->bounds, &blur_bounds))
    return;

  surface = cairo_surface_create_similar_image (cairo_get_target (cr),
                                                CAIRO_FORMAT_ARGB32,
                                                ceil (blur_bounds.size.width),
                                                ceil (blur_bounds.size.height));
  cairo_surface_set_device_offset (surface,
                                   - blur_bounds.origin.x,
                                   - blur_bounds.origin.y);

  cr2 = cairo_create (surface);
  gsk_render_node_draw_full (self->child, cr2, data);
  cairo_destroy (cr2);

  blur_image_surface (surface, (int) ceil (0.5 * self->radius), 3);
  cairo_surface_mark_dirty (surface);

  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_rectangle (cr,
                   node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_fill (cr);

  cairo_surface_destroy (surface);
}

static void
gsk_blur_node_diff (GskRenderNode *node1,
                    GskRenderNode *node2,
                    GskDiffData   *data)
{
  GskBlurNode *self1 = (GskBlurNode *) node1;
  GskBlurNode *self2 = (GskBlurNode *) node2;

  if (self1->radius == self2->radius)
    {
      cairo_rectangle_int_t rect;
      cairo_region_t *sub;
      int i, n, clip_radius;

      clip_radius = ceil (gsk_cairo_blur_compute_pixels (self1->radius / 2.0));
      sub = cairo_region_create ();
      gsk_render_node_diff (self1->child, self2->child, &(GskDiffData) { sub, data->copies, data->surface });

      n = cairo_region_num_rectangles (sub);
      for (i = 0; i < n; i++)
        {
          cairo_region_get_rectangle (sub, i, &rect);
          rect.x -= clip_radius;
          rect.y -= clip_radius;
          rect.width += 2 * clip_radius;
          rect.height += 2 * clip_radius;
          cairo_region_union_rectangle (data->region, &rect);
        }
      cairo_region_destroy (sub);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static void
gsk_blur_node_render_opacity (GskRenderNode  *node,
                              GskOpacityData *data)
{
  GskBlurNode *self = (GskBlurNode *) node;
  GskOpacityData child_data = GSK_OPACITY_DATA_INIT_EMPTY (data->copies);

  gsk_render_node_render_opacity (self->child, &child_data);

  if (!gsk_rect_is_empty (&child_data.opaque))
    {
      float clip_radius = gsk_cairo_blur_compute_pixels (self->radius / 2.0);

      graphene_rect_inset (&child_data.opaque, clip_radius, clip_radius);

      if (!gsk_rect_is_empty (&child_data.opaque))
        {
          if (gsk_rect_is_empty (&data->opaque))
            data->opaque = child_data.opaque;
          else
            gsk_rect_coverage (&data->opaque, &child_data.opaque, &data->opaque);
        }
    }
}

static GskRenderNode **
gsk_blur_node_get_children (GskRenderNode *node,
                            gsize         *n_children)
{
  GskBlurNode *self = (GskBlurNode *) node;

  *n_children = 1;
  
  return &self->child;
}

static GskRenderNode *
gsk_blur_node_replay (GskRenderNode   *node,
                      GskRenderReplay *replay)
{
  GskBlurNode *self = (GskBlurNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_blur_node_new (child, self->radius);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_blur_node_class_init (gpointer g_class,
                          gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_BLUR_NODE;

  node_class->finalize = gsk_blur_node_finalize;
  node_class->draw = gsk_blur_node_draw;
  node_class->diff = gsk_blur_node_diff;
  node_class->get_children = gsk_blur_node_get_children;
  node_class->replay = gsk_blur_node_replay;
  node_class->render_opacity = gsk_blur_node_render_opacity;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskBlurNode, gsk_blur_node)

/**
 * gsk_blur_node_new:
 * @child: the child node to blur
 * @radius: the blur radius. Must be positive
 *
 * Creates a render node that blurs the child.
 *
 * Returns: (transfer full) (type GskBlurNode): a new `GskRenderNode`
 */
GskRenderNode *
gsk_blur_node_new (GskRenderNode *child,
                   float          radius)
{
  GskBlurNode *self;
  GskRenderNode *node;
  float clip_radius;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (radius >= 0, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_BLUR_NODE);
  node = (GskRenderNode *) self;

  self->child = gsk_render_node_ref (child);
  self->radius = radius;

  clip_radius = gsk_cairo_blur_compute_pixels (radius / 2.0);

  gsk_rect_init_from_rect (&node->bounds, &child->bounds);
  graphene_rect_inset (&self->render_node.bounds, - clip_radius, - clip_radius);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);

  return node;
}

/**
 * gsk_blur_node_get_child:
 * @node: (type GskBlurNode): a blur `GskRenderNode`
 *
 * Retrieves the child `GskRenderNode` of the blur @node.
 *
 * Returns: (transfer none): the blurred child node
 */
GskRenderNode *
gsk_blur_node_get_child (const GskRenderNode *node)
{
  const GskBlurNode *self = (const GskBlurNode *) node;

  return self->child;
}

/**
 * gsk_blur_node_get_radius:
 * @node: (type GskBlurNode): a blur `GskRenderNode`
 *
 * Retrieves the blur radius of the @node.
 *
 * Returns: the blur radius
 */
float
gsk_blur_node_get_radius (const GskRenderNode *node)
{
  const GskBlurNode *self = (const GskBlurNode *) node;

  return self->radius;
}

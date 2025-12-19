/* GSK - The GTK Scene Kit
 *
 * Copyright 2025  Benjamin Otte
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

#include "gskdisplacementnodeprivate.h"

#include "gskcontainernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

#include "gdk/gdkcairoprivate.h"

#if 0
#define DEBUG g_print
#else
#define DEBUG(...)
#endif

/*< private >
 * GskDisplacementNode:
 *
 * A render node that uses a displacement map to offset each pixel of the
 * child.
 *
 * Since: 4.22
 */
struct _GskDisplacementNode
{
  GskRenderNode render_node;

  union {
    GskRenderNode *children[2];
    struct {
      GskRenderNode *child;
      GskRenderNode *displacement;
    };
  };
  guint channels[2];
  graphene_size_t max;
  graphene_size_t scale;
  graphene_point_t offset;
};

static void
gsk_displacement_node_finalize (GskRenderNode *node)
{
  GskDisplacementNode *self = (GskDisplacementNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_DISPLACEMENT_NODE));

  gsk_render_node_unref (self->child);
  gsk_render_node_unref (self->displacement);

  parent_class->finalize (node);
}

static float
get_channel (guint32 pixel,
             guint   channel)
{
  float alpha = (pixel >> 24) / 255.0;
  float result;

  if (channel == 3)
    return alpha;
  
  if (alpha == 0.0f)
    return 0.0f;

  result = (pixel >> 8 * (2 - channel)) & 0xFF;
  result = result / 255.0f;
  /* unpremultiply */
  result /= alpha;
  return result;
}

static guint32
sample_image (guchar *image,
              gsize   width,
              gsize   height,
              gsize   stride,
              float   x,
              float   y)
{
  int ix, iy;

  ix = round (x);
  iy = round (y);

  if (ix < 0 || ix >= width ||
      iy < 0 || iy >= height)
    return 0; /* transparent */

  return *(guint32 *) (image + iy * stride + ix * sizeof (guint32));
}

static void
cairo_surface_get_device_matrix (cairo_surface_t *surface,
                                 cairo_matrix_t  *matrix)
{
  cairo_surface_get_device_scale (surface, &matrix->xx, &matrix->yy);
  cairo_surface_get_device_offset (surface, &matrix->x0, &matrix->y0);
  matrix->yx = matrix->xy = 0;
}

static void
get_matrix (cairo_pattern_t *pattern,
            cairo_surface_t *surface,
            cairo_matrix_t  *out_matrix)
{
  cairo_matrix_t pattern_matrix, surface_matrix;

  cairo_pattern_get_matrix (pattern, &pattern_matrix);
  cairo_surface_get_device_matrix (surface, &surface_matrix);

  DEBUG ("pattern: %g %g %g\n         %g %g %g\n",
           pattern_matrix.xx, pattern_matrix.yx, pattern_matrix.x0,
           pattern_matrix.yx, pattern_matrix.yy, pattern_matrix.y0);
  DEBUG ("surface: %g %g %g\n         %g %g %g\n",
           surface_matrix.xx, surface_matrix.yx, surface_matrix.x0,
           surface_matrix.yx, surface_matrix.yy, surface_matrix.y0);

  cairo_matrix_multiply (out_matrix, &pattern_matrix, &surface_matrix);
}

static void
apply_displacement (GskDisplacementNode *self,
                    cairo_surface_t     *image_surface,
                    cairo_pattern_t     *displacement_pattern)
{
  cairo_surface_t *d_surface, *d_image, *i_image;
  cairo_matrix_t d_matrix, i_matrix;
  guchar *d_data, *i_data;
  gsize x, y, d_width, d_height, d_stride, i_width, i_height, i_stride;
  double s, t;
  float h, v;
  guint32* pixel_data;

  cairo_pattern_get_surface (displacement_pattern, &d_surface);
  d_image = cairo_surface_map_to_image (d_surface, NULL);
  g_assert (cairo_image_surface_get_format (d_image) == CAIRO_FORMAT_ARGB32);
  get_matrix (displacement_pattern, d_surface, &d_matrix);
  d_data = cairo_image_surface_get_data (d_image);
  d_width = cairo_image_surface_get_width (d_image);
  d_height = cairo_image_surface_get_height (d_image);
  d_stride = cairo_image_surface_get_stride (d_image);
  DEBUG ("displacement image: %zu %zu\n", d_width, d_height);

  i_image = cairo_surface_map_to_image (image_surface, NULL);
  g_assert (cairo_image_surface_get_format (i_image) == CAIRO_FORMAT_ARGB32);
  cairo_surface_get_device_matrix (image_surface, &i_matrix);
  DEBUG ("image:   %g %g %g\n         %g %g %g\n",
           i_matrix.xx, i_matrix.yx, i_matrix.x0,
           i_matrix.yx, i_matrix.yy, i_matrix.y0);
  i_data = cairo_image_surface_get_data (i_image);
  i_width = cairo_image_surface_get_width (i_image);
  i_height = cairo_image_surface_get_height (i_image);
  i_stride = cairo_image_surface_get_stride (i_image);
  DEBUG ("child image: %zu %zu\n", i_width, i_height);

  cairo_matrix_invert (&d_matrix);

  for (y = 0; y < d_height; y++)
    {
      pixel_data = (guint32 *) d_data;
      for (x = 0; x < d_width; x++)
        {
          s = x;
          t = y;
          DEBUG ("pixel       : %08X\n", pixel_data[x]);
          DEBUG ("coordinate  : %zu %zu\n", x, y);
          cairo_matrix_transform_point (&d_matrix, &s, &t);
          DEBUG ("node space  : %g %g\n", s, t);
          h = get_channel (pixel_data[x], self->channels[0]);
          v = get_channel (pixel_data[x], self->channels[1]);
          DEBUG ("displacement: %g %g\n", h, v);
          h = CLAMP (self->scale.width * (h - self->offset.x), -self->max.width, self->max.width);
          v = CLAMP (self->scale.height * (v - self->offset.y), -self->max.height, self->max.height);
          DEBUG ("transformed : %g %g\n", h, v);
          s += h;
          t += v;
          DEBUG ("coordinate  : %g %g\n", s, t);
          cairo_matrix_transform_point (&i_matrix, &s, &t);
          DEBUG ("final       : %g %g\n", s, t);

          pixel_data[x] = sample_image (i_data, i_width, i_height, i_stride, s, t);
          DEBUG ("result      : %08X\n", pixel_data[x]);
        }
      d_data += d_stride;
    }

  cairo_surface_mark_dirty (d_image);
  cairo_surface_unmap_image (d_surface, d_image);
  /* https://gitlab.freedesktop.org/cairo/cairo/-/merge_requests/487 */
  cairo_surface_mark_dirty (d_surface);
  cairo_surface_write_to_png (d_surface, "foo.png");
  cairo_surface_unmap_image (image_surface, i_image);
}

static void
gsk_displacement_node_draw (GskRenderNode *node,
                            cairo_t       *cr,
                            GskCairoData  *data)
{
  GskDisplacementNode *self = (GskDisplacementNode *) node;
  cairo_pattern_t *displacement;
  cairo_surface_t *child_surface;
  cairo_t *child_cr;
  graphene_rect_t child_bounds;

  child_bounds = node->bounds;
  graphene_rect_inset (&child_bounds, - self->max.width, - self->max.height);
  if (!gsk_rect_intersection (&child_bounds, &self->child->bounds, &child_bounds))
    return;

  /* clip so the push_group() creates a smaller surface */
  gdk_cairo_rect (cr, &node->bounds);
  cairo_clip (cr);
  if (gdk_cairo_is_all_clipped (cr))
    return;

  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
  gsk_render_node_draw_full (self->displacement, cr, data);
  displacement = cairo_pop_group (cr);

  child_surface = gdk_cairo_create_similar_surface (cr, CAIRO_CONTENT_COLOR_ALPHA, &child_bounds);
  child_cr = cairo_create (child_surface);
  gsk_render_node_draw_full (self->child, child_cr, data);
  cairo_destroy (child_cr);

  apply_displacement (self, child_surface, displacement);

  cairo_set_source (cr, displacement);
  cairo_paint (cr);

  cairo_surface_destroy (child_surface);
  cairo_pattern_destroy (displacement);
}

static void
gdk_cairo_region_grow (cairo_region_t *region,
                       int             grow_x,
                       int             grow_y)
{
  cairo_region_t *tmp;
  guint i, n;

  n = cairo_region_num_rectangles (region);
  tmp = cairo_region_create ();

  for (i = 0; i < n; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (region, i, &rect);
      rect.x -= grow_x;
      rect.y -= grow_y;
      rect.width += 2 * grow_x;
      rect.height += 2 * grow_y;
      cairo_region_union_rectangle (tmp ,&rect);
    }
  
  cairo_region_subtract (region, region);
  cairo_region_union (region, tmp);
  cairo_region_destroy (tmp);
}

static void
gsk_displacement_node_diff (GskRenderNode *node1,
                            GskRenderNode *node2,
                            GskDiffData   *data)
{
  GskDisplacementNode *self1 = (GskDisplacementNode *) node1;
  GskDisplacementNode *self2 = (GskDisplacementNode *) node2;
  cairo_region_t *child_region, *displacement_region;

  if (!gsk_rect_equal (&node1->bounds, &node2->bounds) ||
      self1->channels[0] != self2->channels[1] ||
      self1->channels[1] != self2->channels[1] ||
      !graphene_size_equal (&self1->max, &self2->max) ||
      !graphene_size_equal (&self1->scale, &self2->scale) ||
      !graphene_point_equal (&self1->offset, &self2->offset))
    {
      gsk_render_node_diff_impossible (node1, node2, data);
      return;
    }

  child_region = cairo_region_create ();
  gsk_render_node_diff (self1->child, self2->child, &(GskDiffData) { child_region, data->copies, data->surface });
  displacement_region = cairo_region_create ();
  gsk_render_node_diff (self1->displacement, self2->displacement, &(GskDiffData) { displacement_region, data->copies, data->surface });
  cairo_region_union (child_region, displacement_region);
  gdk_cairo_region_grow (child_region, ceilf (self1->max.width), ceilf (self1->max.height));
  cairo_region_union (data->region, child_region);

  cairo_region_destroy (displacement_region);
  cairo_region_destroy (child_region);
}

static GskRenderNode **
gsk_displacement_node_get_children (GskRenderNode *node,
                                    gsize         *n_children)
{
  GskDisplacementNode *self = (GskDisplacementNode *) node;

  *n_children = G_N_ELEMENTS (self->children);

  return self->children;
}

static GskRenderNode *
gsk_displacement_node_replay (GskRenderNode   *node,
                              GskRenderReplay *replay)
{
  GskDisplacementNode *self = (GskDisplacementNode *) node;
  GskRenderNode *result, *child, *displacement;

  child = gsk_render_replay_filter_node (replay, self->child);
  displacement = gsk_render_replay_filter_node (replay, self->displacement);

  if (displacement == NULL)
    displacement = gsk_container_node_new (NULL, 0);
  if (child == NULL)
    child = gsk_container_node_new (NULL, 0);

  if (child == self->child && displacement == self->displacement)
    result = gsk_render_node_ref (node);
  else
    result = gsk_displacement_node_new (&node->bounds, child, displacement, self->channels, &self->max, &self->scale, &self->offset);

  gsk_render_node_unref (child);
  gsk_render_node_unref (displacement);

  return result;
}

static void
gsk_displacement_node_render_opacity (GskRenderNode  *node,
                                      GskOpacityData *data)
{
  GskDisplacementNode *self = (GskDisplacementNode *) node;
  GskOpacityData child_data = GSK_OPACITY_DATA_INIT_EMPTY (data->copies);

  gsk_render_node_render_opacity (self->child, &child_data);

  if (!gsk_rect_is_empty (&child_data.opaque))
    {
      graphene_rect_inset (&child_data.opaque, self->max.width, self->max.height);
      if (gsk_rect_is_empty (&data->opaque))
        data->opaque = child_data.opaque;
      else
        gsk_rect_coverage (&data->opaque, &child_data.opaque, &data->opaque);
    }
}

static void
gsk_displacement_node_class_init (gpointer g_class,
                                  gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_DISPLACEMENT_NODE;

  node_class->finalize = gsk_displacement_node_finalize;
  node_class->draw = gsk_displacement_node_draw;
  node_class->diff = gsk_displacement_node_diff;
  node_class->get_children = gsk_displacement_node_get_children;
  node_class->replay = gsk_displacement_node_replay;
  node_class->render_opacity = gsk_displacement_node_render_opacity;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskDisplacementNode, gsk_displacement_node)

/*< private >
 * gsk_displacement_node_new:
 * @bounds: The rectangle to apply to
 * @child: The child to displace
 * @displacement: The dissplacement mask
 * @channels: Which channels to usefor the displacement in horizontal and
 *   vertical direction respectively. The numbers must be from 0 to 3 for
 *   red, green, blue, and alpha channel respectively.
 * @max: The maximum displacement in units
 * @scale: The scale to apply to the displacement value
 * @offset: The offset to apply to the displacement value
 *
 * Creates a `GskRenderNode` that will displace the child according to the
 * displacement mask. This is modeled after [SVG's feDisplacementMap
 * filter](https://www.w3.org/TR/SVG11/filters.html#feDisplacementMapElement).
 *
 * The amount to displace is determine by sampling the displacement
 * at every coordinate, converting its value into the given colorstate and
 * applying the formula `value = scale * (value - offset)` and clamping the
 * resulting value to be between `-max` and `max`.
 *
 * Returns: (transfer full) (type GskDisplacementNode): A new `GskRenderNode`
 *
 * Since: 4.22
 */
GskRenderNode *
gsk_displacement_node_new (const graphene_rect_t  *bounds,
                           GskRenderNode          *child,
                           GskRenderNode          *displacement,
                           const guint             channels[2],
                           const graphene_size_t  *max,
                           const graphene_size_t  *scale,
                           const graphene_point_t *offset)
{
  GskDisplacementNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (displacement), NULL);
  g_return_val_if_fail (channels[0] < 4, NULL);
  g_return_val_if_fail (channels[1] < 4, NULL);
  g_return_val_if_fail (max->width > 0, NULL);
  g_return_val_if_fail (max->height > 0, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_DISPLACEMENT_NODE);
  node = (GskRenderNode *) self;
  node->bounds = *bounds;
  self->child = gsk_render_node_ref (child);
  self->displacement = gsk_render_node_ref (displacement);
  self->channels[0] = channels[0];
  self->channels[1] = channels[1];
  self->max = *max;
  self->scale = *scale;
  self->offset = *offset;

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  if (child->fully_opaque)
    {
      graphene_rect_t child_opaque = child->bounds;
      graphene_rect_inset (&child_opaque, self->max.width, self->max.height);
      node->fully_opaque = gsk_rect_contains_rect (&child_opaque, &node->bounds);
    }
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child) ||
                                   gsk_render_node_contains_subsurface_node (displacement);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child) ||
                              gsk_render_node_contains_paste_node (displacement);

  return node;
}

/*< private >
 * gsk_displacement_node_get_child:
 * @node: (type GskDisplacementNode): a displacement `GskRenderNode`
 *
 * Gets the child node that is getting displaced by the given @node.
 *
 * Returns: (transfer none): the child `GskRenderNode`
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_displacement_node_get_child (const GskRenderNode *node)
{
  const GskDisplacementNode *self = (const GskDisplacementNode *) node;

  return self->child;
}

/*< private >
 * gsk_displacement_node_get_displacement:
 * @node: (type GskDisplacementNode): a displacement `GskRenderNode`
 *
 * Gets the node that defines the displacement mask.
 *
 * Returns: (transfer none): the displacement `GskRenderNode`
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_displacement_node_get_displacement (const GskRenderNode *node)
{
  const GskDisplacementNode *self = (const GskDisplacementNode *) node;

  return self->displacement;
}

const guint *
gsk_displacement_node_get_channels (const GskRenderNode *node)
{
  const GskDisplacementNode *self = (const GskDisplacementNode *) node;

  return self->channels;
}

const graphene_size_t *
gsk_displacement_node_get_max (const GskRenderNode *node)
{
  const GskDisplacementNode *self = (const GskDisplacementNode *) node;

  return &self->max;
}

const graphene_size_t *
gsk_displacement_node_get_scale (const GskRenderNode *node)
{
  const GskDisplacementNode *self = (const GskDisplacementNode *) node;

  return &self->scale;
}

const graphene_point_t *
gsk_displacement_node_get_offset (const GskRenderNode *node)
{
  const GskDisplacementNode *self = (const GskDisplacementNode *) node;

  return &self->offset;
}


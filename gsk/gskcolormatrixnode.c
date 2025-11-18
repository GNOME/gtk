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

#include "gskcolormatrixnodeprivate.h"

#include "gskrectprivate.h"
#include "gskrenderreplay.h"

#include "gskrendernodeprivate.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskColorMatrixNode:
 *
 * A render node controlling the color matrix of its single child node.
 */
struct _GskColorMatrixNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  graphene_matrix_t color_matrix;
  graphene_vec4_t color_offset;
};

static void
gsk_color_matrix_node_finalize (GskRenderNode *node)
{
  GskColorMatrixNode *self = (GskColorMatrixNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_COLOR_MATRIX_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

void
apply_color_matrix_to_pattern (cairo_pattern_t         *pattern,
                               const graphene_matrix_t *color_matrix,
                               const graphene_vec4_t   *color_offset)
{
  cairo_surface_t *surface, *image_surface;
  guchar *data;
  gsize x, y, width, height, stride;
  float alpha;
  graphene_vec4_t pixel;
  guint32* pixel_data;

  cairo_pattern_get_surface (pattern, &surface);
  image_surface = cairo_surface_map_to_image (surface, NULL);

  data = cairo_image_surface_get_data (image_surface);
  width = cairo_image_surface_get_width (image_surface);
  height = cairo_image_surface_get_height (image_surface);
  stride = cairo_image_surface_get_stride (image_surface);

  for (y = 0; y < height; y++)
    {
      pixel_data = (guint32 *) data;
      for (x = 0; x < width; x++)
        {
          alpha = ((pixel_data[x] >> 24) & 0xFF) / 255.0;

          if (alpha == 0)
            {
              graphene_vec4_init (&pixel, 0.0, 0.0, 0.0, 0.0);
            }
          else
            {
              graphene_vec4_init (&pixel,
                                  ((pixel_data[x] >> 16) & 0xFF) / (255.0 * alpha),
                                  ((pixel_data[x] >>  8) & 0xFF) / (255.0 * alpha),
                                  ( pixel_data[x]        & 0xFF) / (255.0 * alpha),
                                  alpha);
              graphene_matrix_transform_vec4 (color_matrix, &pixel, &pixel);
            }

          graphene_vec4_add (&pixel, color_offset, &pixel);

          alpha = graphene_vec4_get_w (&pixel);

          if (alpha > 0.0)
            {
              alpha = MIN (alpha, 1.0);
              pixel_data[x] = (((guint32) roundf (alpha * 255)) << 24) |
                              (((guint32) roundf (CLAMP (graphene_vec4_get_x (&pixel), 0, 1) * alpha * 255)) << 16) |
                              (((guint32) roundf (CLAMP (graphene_vec4_get_y (&pixel), 0, 1) * alpha * 255)) <<  8) |
                               ((guint32) roundf (CLAMP (graphene_vec4_get_z (&pixel), 0, 1) * alpha * 255));
            }
          else
            {
              pixel_data[x] = 0;
            }
        }
      data += stride;
    }

  cairo_surface_mark_dirty (image_surface);
  cairo_surface_unmap_image (surface, image_surface);
  /* https://gitlab.freedesktop.org/cairo/cairo/-/merge_requests/487 */
  cairo_surface_mark_dirty (surface);
}

static void
gsk_color_matrix_node_draw (GskRenderNode *node,
                            cairo_t       *cr,
                            GdkColorState *ccs)
{
  GskColorMatrixNode *self = (GskColorMatrixNode *) node;
  cairo_pattern_t *pattern;

  /* clip so the push_group() creates a smaller surface */
  gdk_cairo_rect (cr, &node->bounds);
  cairo_clip (cr);

  if (gdk_cairo_is_all_clipped (cr))
    return;

  cairo_push_group (cr);

  gsk_render_node_draw_ccs (self->child, cr, ccs);

  pattern = cairo_pop_group (cr);
  apply_color_matrix_to_pattern (pattern, &self->color_matrix, &self->color_offset);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_pattern_destroy (pattern);
}

static void
gsk_color_matrix_node_diff (GskRenderNode *node1,
                            GskRenderNode *node2,
                            GskDiffData   *data)
{
  GskColorMatrixNode *self1 = (GskColorMatrixNode *) node1;
  GskColorMatrixNode *self2 = (GskColorMatrixNode *) node2;

  if (!graphene_vec4_equal (&self1->color_offset, &self2->color_offset))
    goto nope;

  if (!graphene_matrix_equal_fast (&self1->color_matrix, &self2->color_matrix))
    goto nope;

  gsk_render_node_diff (self1->child, self2->child, data);
  return;

nope:
  gsk_render_node_diff_impossible (node1, node2, data);
  return;
}

static GskRenderNode *
gsk_color_matrix_node_replay (GskRenderNode   *node,
                              GskRenderReplay *replay)
{
  GskColorMatrixNode *self = (GskColorMatrixNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_color_matrix_node_new (child, &self->color_matrix, &self->color_offset);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_color_matrix_node_class_init (gpointer g_class,
                                  gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_COLOR_MATRIX_NODE;

  node_class->finalize = gsk_color_matrix_node_finalize;
  node_class->draw = gsk_color_matrix_node_draw;
  node_class->diff = gsk_color_matrix_node_diff;
  node_class->replay = gsk_color_matrix_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskColorMatrixNode, gsk_color_matrix_node)

/**
 * gsk_color_matrix_node_new:
 * @child: The node to draw
 * @color_matrix: The matrix to apply
 * @color_offset: Values to add to the color
 *
 * Creates a `GskRenderNode` that will drawn the @child with
 * @color_matrix.
 *
 * In particular, the node will transform colors by applying
 *
 *     pixel = transpose(color_matrix) * pixel + color_offset
 *
 * for every pixel. The transformation operates on unpremultiplied
 * colors, with color components ordered R, G, B, A.
 *
 * Returns: (transfer full) (type GskColorMatrixNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_color_matrix_node_new (GskRenderNode           *child,
                           const graphene_matrix_t *color_matrix,
                           const graphene_vec4_t   *color_offset)
{
  GskColorMatrixNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_COLOR_MATRIX_NODE);
  node = (GskRenderNode *) self;

  self->child = gsk_render_node_ref (child);
  graphene_matrix_init_from_matrix (&self->color_matrix, color_matrix);
  graphene_vec4_init_from_vec4 (&self->color_offset, color_offset);

  gsk_rect_init_from_rect (&node->bounds, &child->bounds);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);

  return node;
}

/**
 * gsk_color_matrix_node_get_child:
 * @node: (type GskColorMatrixNode): a color matrix `GskRenderNode`
 *
 * Gets the child node that is getting its colors modified by the given @node.
 *
 * Returns: (transfer none): The child that is getting its colors modified
 **/
GskRenderNode *
gsk_color_matrix_node_get_child (const GskRenderNode *node)
{
  const GskColorMatrixNode *self = (const GskColorMatrixNode *) node;

  return self->child;
}

/**
 * gsk_color_matrix_node_get_color_matrix:
 * @node: (type GskColorMatrixNode): a color matrix `GskRenderNode`
 *
 * Retrieves the color matrix used by the @node.
 *
 * Returns: a 4x4 color matrix
 */
const graphene_matrix_t *
gsk_color_matrix_node_get_color_matrix (const GskRenderNode *node)
{
  const GskColorMatrixNode *self = (const GskColorMatrixNode *) node;

  return &self->color_matrix;
}

/**
 * gsk_color_matrix_node_get_color_offset:
 * @node: (type GskColorMatrixNode): a color matrix `GskRenderNode`
 *
 * Retrieves the color offset used by the @node.
 *
 * Returns: a color vector
 */
const graphene_vec4_t *
gsk_color_matrix_node_get_color_offset (const GskRenderNode *node)
{
  const GskColorMatrixNode *self = (const GskColorMatrixNode *) node;

  return &self->color_offset;
}

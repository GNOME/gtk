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

#include "gskmasknode.h"

#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

#include "gskcolormatrixnodeprivate.h"
#include "gskrectprivate.h"
#include "gdk/gdkcairoprivate.h"

/**
 * GskMaskNode:
 *
 * A render node masking one child node with another.
 *
 * Since: 4.10
 */
typedef struct _GskMaskNode GskMaskNode;

struct _GskMaskNode
{
  GskRenderNode render_node;

  union {
    GskRenderNode *children[2];
    struct {
      GskRenderNode *source;
      GskRenderNode *mask;
    };
  };
  GskMaskMode mask_mode;
};

static void
gsk_mask_node_finalize (GskRenderNode *node)
{
  GskMaskNode *self = (GskMaskNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_MASK_NODE));

  gsk_render_node_unref (self->source);
  gsk_render_node_unref (self->mask);

  parent_class->finalize (node);
}

static void
apply_luminance_to_pattern (cairo_pattern_t *pattern,
                            gboolean         invert_luminance)
{
  cairo_surface_t *surface, *image_surface;
  guchar *data;
  gsize x, y, width, height, stride;
  guint red, green, blue, alpha, luminance;
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
          alpha = (pixel_data[x] >> 24) & 0xFF;
          red   = (pixel_data[x] >> 16) & 0xFF;
          green = (pixel_data[x] >>  8) & 0xFF;
          blue  = (pixel_data[x] >>  0) & 0xFF;

          luminance = 2126 * red + 7152 * green + 722 * blue;
          if (invert_luminance)
            luminance = 10000 * alpha - luminance;
          luminance = (luminance + 5000) / 10000;

          pixel_data[x] = luminance * 0x1010101;
        }
      data += stride;
    }

  cairo_surface_mark_dirty (image_surface);
  cairo_surface_unmap_image (surface, image_surface);
  /* https://gitlab.freedesktop.org/cairo/cairo/-/merge_requests/487 */
  cairo_surface_mark_dirty (surface);
}

static void
gsk_mask_node_draw (GskRenderNode *node,
                    cairo_t       *cr,
                    GskCairoData  *data)
{
  GskMaskNode *self = (GskMaskNode *) node;
  cairo_pattern_t *mask_pattern;
  graphene_matrix_t color_matrix;
  graphene_vec4_t color_offset;

  /* clip so the push_group() creates a smaller surface */
  gdk_cairo_rectangle_snap_to_grid (cr, &node->bounds);
  cairo_clip (cr);

  if (gdk_cairo_is_all_clipped (cr))
    return;

  cairo_push_group (cr);
  gsk_render_node_draw_full (self->source, cr, data);
  cairo_pop_group_to_source (cr);

  cairo_push_group (cr);
  gsk_render_node_draw_full (self->mask, cr, data);
  mask_pattern = cairo_pop_group (cr);

  switch (self->mask_mode)
    {
    case GSK_MASK_MODE_ALPHA:
      break;
    case GSK_MASK_MODE_INVERTED_ALPHA:
      graphene_matrix_init_from_float (&color_matrix, (float[]){  0,  0,  0,  0,
                                                                  0,  0,  0,  0,
                                                                  0,  0,  0,  0,
                                                                 -1, -1, -1, -1 });
      graphene_vec4_init (&color_offset, 1, 1, 1, 1);
      apply_color_matrix_to_pattern (mask_pattern, &color_matrix, &color_offset, data->ccs, data);
      break;
    case GSK_MASK_MODE_LUMINANCE:
      apply_luminance_to_pattern (mask_pattern, FALSE);
      break;
    case GSK_MASK_MODE_INVERTED_LUMINANCE:
      apply_luminance_to_pattern (mask_pattern, TRUE);
      break;
    default:
      g_assert_not_reached ();
    }

  cairo_mask (cr, mask_pattern);

  cairo_pattern_destroy (mask_pattern);
}

static void
gsk_mask_node_diff (GskRenderNode *node1,
                    GskRenderNode *node2,
                    GskDiffData   *data)
{
  GskMaskNode *self1 = (GskMaskNode *) node1;
  GskMaskNode *self2 = (GskMaskNode *) node2;

  if (self1->mask_mode != self2->mask_mode)
    {
      gsk_render_node_diff_impossible (node1, node2, data);
      return;
    }

  gsk_render_node_diff (self1->source, self2->source, data);
  gsk_render_node_diff (self1->mask, self2->mask, data);
}

static GskRenderNode **
gsk_mask_node_get_children (GskRenderNode *node,
                            gsize         *n_children)
{
  GskMaskNode *self = (GskMaskNode *) node;

  *n_children = G_N_ELEMENTS (self->children);

  return self->children;
}

static GskRenderNode *
gsk_mask_node_replay (GskRenderNode   *node,
                      GskRenderReplay *replay)
{
  GskMaskNode *self = (GskMaskNode *) node;
  GskRenderNode *result, *source, *mask;

  source = gsk_render_replay_filter_node (replay, self->source);
  mask = gsk_render_replay_filter_node (replay, self->mask);

  if (source == NULL)
    {
      g_clear_pointer (&mask, gsk_render_node_unref);
      return NULL;
    }

  if (mask == NULL)
    {
      if (self->mask_mode == GSK_MASK_MODE_INVERTED_ALPHA)
        result = gsk_render_node_ref (source);
      else
        result = NULL;
    }
  else if (source == self->source && mask == self->mask)
    {
      result = gsk_render_node_ref (node);
    }
  else
    {
      result = gsk_mask_node_new (source, mask, self->mask_mode);
    }

  g_clear_pointer (&source, gsk_render_node_unref);
  g_clear_pointer (&mask, gsk_render_node_unref);

  return result;
}

static void
gsk_mask_node_class_init (gpointer g_class,
                          gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_MASK_NODE;

  node_class->finalize = gsk_mask_node_finalize;
  node_class->draw = gsk_mask_node_draw;
  node_class->diff = gsk_mask_node_diff;
  node_class->get_children = gsk_mask_node_get_children;
  node_class->replay = gsk_mask_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskMaskNode, gsk_mask_node)

/**
 * gsk_mask_node_new:
 * @source: The source node to be drawn
 * @mask: The node to be used as mask
 * @mask_mode: The mask mode to use
 *
 * Creates a `GskRenderNode` that will mask a given node by another.
 *
 * The @mask_mode determines how the 'mask values' are derived from
 * the colors of the @mask. Applying the mask consists of multiplying
 * the 'mask value' with the alpha of the source.
 *
 * Returns: (transfer full) (type GskMaskNode): A new `GskRenderNode`
 *
 * Since: 4.10
 */
GskRenderNode *
gsk_mask_node_new (GskRenderNode *source,
                   GskRenderNode *mask,
                   GskMaskMode    mask_mode)
{
  GskMaskNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (source), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (mask), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_MASK_NODE);
  node = (GskRenderNode *) self;
  self->source = gsk_render_node_ref (source);
  self->mask = gsk_render_node_ref (mask);
  self->mask_mode = mask_mode;

  if (mask_mode == GSK_MASK_MODE_INVERTED_ALPHA)
    node->bounds = source->bounds;
  else if (!gsk_rect_intersection (&source->bounds, &mask->bounds, &node->bounds))
    node->bounds = *graphene_rect_zero ();

  node->preferred_depth = gsk_render_node_get_preferred_depth (source);
  node->is_hdr = gsk_render_node_is_hdr (source) ||
                 gsk_render_node_is_hdr (mask);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (source) ||
                                   gsk_render_node_contains_subsurface_node (mask);
  node->contains_paste_node = gsk_render_node_contains_paste_node (source) ||
                              gsk_render_node_contains_paste_node (mask);
  node->needs_blending = gsk_render_node_needs_blending (source);

  return node;
}

/**
 * gsk_mask_node_get_source:
 * @node: (type GskMaskNode): a mask `GskRenderNode`
 *
 * Retrieves the source `GskRenderNode` child of the @node.
 *
 * Returns: (transfer none): the source child node
 *
 * Since: 4.10
 */
GskRenderNode *
gsk_mask_node_get_source (const GskRenderNode *node)
{
  const GskMaskNode *self = (const GskMaskNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_MASK_NODE), NULL);

  return self->source;
}

/**
 * gsk_mask_node_get_mask:
 * @node: (type GskMaskNode): a mask `GskRenderNode`
 *
 * Retrieves the mask `GskRenderNode` child of the @node.
 *
 * Returns: (transfer none): the mask child node
 *
 * Since: 4.10
 */
GskRenderNode *
gsk_mask_node_get_mask (const GskRenderNode *node)
{
  const GskMaskNode *self = (const GskMaskNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_MASK_NODE), NULL);

  return self->mask;
}

/**
 * gsk_mask_node_get_mask_mode:
 * @node: (type GskMaskNode): a blending `GskRenderNode`
 *
 * Retrieves the mask mode used by @node.
 *
 * Returns: the mask mode
 *
 * Since: 4.10
 */
GskMaskMode
gsk_mask_node_get_mask_mode (const GskRenderNode *node)
{
  const GskMaskNode *self = (const GskMaskNode *) node;

  return self->mask_mode;
}

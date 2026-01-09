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

#include "gsktexturescalenode.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrenderreplay.h"

#include "gdk/gdkcairoprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdktexturedownloaderprivate.h"

/**
 * GskTextureScaleNode:
 *
 * A render node for a `GdkTexture`, with control over scaling.
 *
 * Since: 4.10
 */
struct _GskTextureScaleNode
{
  GskRenderNode render_node;

  GdkTexture *texture;
  GskScalingFilter filter;
};

static void
gsk_texture_scale_node_finalize (GskRenderNode *node)
{
  GskTextureScaleNode *self = (GskTextureScaleNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_TEXTURE_SCALE_NODE));

  g_clear_object (&self->texture);

  parent_class->finalize (node);
}

static void
gsk_texture_scale_node_draw (GskRenderNode *node,
                             cairo_t       *cr,
                             GskCairoData  *data)
{
  GskTextureScaleNode *self = (GskTextureScaleNode *) node;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;
  cairo_filter_t filters[] = {
    CAIRO_FILTER_BILINEAR,
    CAIRO_FILTER_NEAREST,
    CAIRO_FILTER_GOOD,
  };
  cairo_t *cr2;
  cairo_surface_t *surface2;
  graphene_rect_t clip_rect;

  /* Make sure we draw the minimum region by using the clip */
  gdk_cairo_rect (cr, &node->bounds);
  cairo_clip (cr);
  _graphene_rect_init_from_clip_extents (&clip_rect, cr);
  if (clip_rect.size.width <= 0 || clip_rect.size.height <= 0)
    return;

  surface2 = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                         (int) ceilf (clip_rect.size.width),
                                         (int) ceilf (clip_rect.size.height));
  cairo_surface_set_device_offset (surface2, -clip_rect.origin.x, -clip_rect.origin.y);
  cr2 = cairo_create (surface2);

  surface = gdk_texture_download_surface (self->texture, data->ccs);
  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  cairo_matrix_init_scale (&matrix,
                           gdk_texture_get_width (self->texture) / node->bounds.size.width,
                           gdk_texture_get_height (self->texture) / node->bounds.size.height);
  cairo_matrix_translate (&matrix, -node->bounds.origin.x, -node->bounds.origin.y);
  cairo_pattern_set_matrix (pattern, &matrix);
  cairo_pattern_set_filter (pattern, filters[self->filter]);

  cairo_set_source (cr2, pattern);
  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);

  gdk_cairo_rect (cr2, &node->bounds);
  cairo_fill (cr2);

  cairo_destroy (cr2);

  cairo_save (cr);

  cairo_set_source_surface (cr, surface2, 0, 0);
  cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_PAD);
  cairo_surface_destroy (surface2);

  cairo_paint (cr);

  cairo_restore (cr);
}

static void
gsk_texture_scale_node_diff (GskRenderNode *node1,
                             GskRenderNode *node2,
                             GskDiffData   *data)
{
  GskTextureScaleNode *self1 = (GskTextureScaleNode *) node1;
  GskTextureScaleNode *self2 = (GskTextureScaleNode *) node2;
  cairo_region_t *sub;

  if (!gsk_rect_equal (&node1->bounds, &node2->bounds) ||
      self1->filter != self2->filter ||
      gdk_texture_get_width (self1->texture) != gdk_texture_get_width (self2->texture) ||
      gdk_texture_get_height (self1->texture) != gdk_texture_get_height (self2->texture))
    {
      gsk_render_node_diff_impossible (node1, node2, data);
      return;
    }

  if (self1->texture == self2->texture)
    return;

  sub = cairo_region_create ();
  gdk_texture_diff (self1->texture, self2->texture, sub);
  gdk_cairo_region_union_affine (data->region,
                                 sub,
                                 node1->bounds.size.width / gdk_texture_get_width (self1->texture),
                                 node1->bounds.size.height / gdk_texture_get_height (self1->texture),
                                 node1->bounds.origin.x,
                                 node1->bounds.origin.y);
  cairo_region_destroy (sub);
}

static GskRenderNode *
gsk_texture_scale_node_replay (GskRenderNode   *node,
                               GskRenderReplay *replay)
{
  GskTextureScaleNode *self = (GskTextureScaleNode *) node;
  GdkTexture *texture;
  GskRenderNode *result;

  texture = gsk_render_replay_filter_texture (replay, self->texture);
  if (self->texture == texture)
    {
      g_object_unref (texture);
      return gsk_render_node_ref (node);
    }

  result = gsk_texture_scale_node_new (texture, &node->bounds, self->filter);
  g_object_unref (texture);

  return result;
}

static void
gsk_texture_scale_node_class_init (gpointer g_class,
                                   gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_TEXTURE_SCALE_NODE;

  node_class->finalize = gsk_texture_scale_node_finalize;
  node_class->draw = gsk_texture_scale_node_draw;
  node_class->diff = gsk_texture_scale_node_diff;
  node_class->replay = gsk_texture_scale_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskTextureScaleNode, gsk_texture_scale_node)

/**
 * gsk_texture_scale_node_get_texture:
 * @node: (type GskTextureScaleNode): a `GskRenderNode` of type %GSK_TEXTURE_SCALE_NODE
 *
 * Retrieves the `GdkTexture` used when creating this `GskRenderNode`.
 *
 * Returns: (transfer none): the `GdkTexture`
 *
 * Since: 4.10
 */
GdkTexture *
gsk_texture_scale_node_get_texture (const GskRenderNode *node)
{
  const GskTextureScaleNode *self = (const GskTextureScaleNode *) node;

  return self->texture;
}

/**
 * gsk_texture_scale_node_get_filter:
 * @node: (type GskTextureScaleNode): a `GskRenderNode` of type %GSK_TEXTURE_SCALE_NODE
 *
 * Retrieves the `GskScalingFilter` used when creating this `GskRenderNode`.
 *
 * Returns: (transfer none): the `GskScalingFilter`
 *
 * Since: 4.10
 */
GskScalingFilter
gsk_texture_scale_node_get_filter (const GskRenderNode *node)
{
  const GskTextureScaleNode *self = (const GskTextureScaleNode *) node;

  return self->filter;
}

/**
 * gsk_texture_scale_node_new:
 * @texture: the texture to scale
 * @bounds: the size of the texture to scale to
 * @filter: how to scale the texture
 *
 * Creates a node that scales the texture to the size given by the
 * bounds using the filter and then places it at the bounds' position.
 *
 * Note that further scaling and other transformations which are
 * applied to the node will apply linear filtering to the resulting
 * texture, as usual.
 *
 * This node is intended for tight control over scaling applied
 * to a texture, such as in image editors and requires the
 * application to be aware of the whole render tree as further
 * transforms may be applied that conflict with the desired effect
 * of this node.
 *
 * Returns: (transfer full) (type GskTextureScaleNode): A new `GskRenderNode`
 *
 * Since: 4.10
 */
GskRenderNode *
gsk_texture_scale_node_new (GdkTexture            *texture,
                            const graphene_rect_t *bounds,
                            GskScalingFilter       filter)
{
  GskTextureScaleNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_TEXTURE_SCALE_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = gdk_memory_format_alpha (gdk_texture_get_format (texture)) == GDK_MEMORY_ALPHA_OPAQUE &&
    bounds->size.width == floor (bounds->size.width) &&
    bounds->size.height == floor (bounds->size.height);
  node->is_hdr = gdk_color_state_is_hdr (gdk_texture_get_color_state (texture));

  self->texture = g_object_ref (texture);
  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);
  self->filter = filter;

  node->preferred_depth = gdk_texture_get_depth (texture);

  return node;
}

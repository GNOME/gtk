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

#include "gsktexturenode.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrenderreplay.h"

#include "gdk/gdkcairoprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdktexturedownloaderprivate.h"

/* for oversized image fallback - we use a smaller size than Cairo
 * actually allows to avoid rounding errors in Cairo
 */
#define MAX_CAIRO_IMAGE_WIDTH 16384
#define MAX_CAIRO_IMAGE_HEIGHT 16384

/**
 * GskTextureNode:
 *
 * A render node for a `GdkTexture`.
 */
struct _GskTextureNode
{
  GskRenderNode render_node;

  GdkTexture *texture;
};

static void
gsk_texture_node_finalize (GskRenderNode *node)
{
  GskTextureNode *self = (GskTextureNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_TEXTURE_NODE));

  g_clear_object (&self->texture);

  parent_class->finalize (node);
}

static void
gsk_texture_node_draw_oversized (GskRenderNode *node,
                                 cairo_t       *cr,
                                 GdkColorState *ccs)
{
  GskTextureNode *self = (GskTextureNode *) node;
  cairo_surface_t *surface;
  int x, y, width, height;
  GdkTextureDownloader downloader;
  GBytes *bytes;
  const guchar *data;
  gsize stride;

  width = gdk_texture_get_width (self->texture);
  height = gdk_texture_get_height (self->texture);
  gdk_texture_downloader_init (&downloader, self->texture);
  gdk_texture_downloader_set_format (&downloader, GDK_MEMORY_DEFAULT);
  bytes = gdk_texture_downloader_download_bytes (&downloader, &stride);
  gdk_texture_downloader_finish (&downloader);
  data = g_bytes_get_data (bytes, NULL);
  gdk_memory_convert_color_state ((guchar *) data,
                                  &GDK_MEMORY_LAYOUT_SIMPLE (
                                      GDK_MEMORY_DEFAULT,
                                      stride,
                                      width,
                                      height
                                  ),
                                  GDK_COLOR_STATE_SRGB,
                                  ccs);

  gdk_cairo_rectangle_snap_to_grid (cr, &node->bounds);
  cairo_clip (cr);

  cairo_push_group (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
  cairo_translate (cr,
                   node->bounds.origin.x,
                   node->bounds.origin.y);
  cairo_scale (cr, 
               node->bounds.size.width / width,
               node->bounds.size.height / height);

  for (x = 0; x < width; x += MAX_CAIRO_IMAGE_WIDTH)
    {
      int tile_width = MIN (MAX_CAIRO_IMAGE_WIDTH, width - x);
      for (y = 0; y < height; y += MAX_CAIRO_IMAGE_HEIGHT)
        {
          int tile_height = MIN (MAX_CAIRO_IMAGE_HEIGHT, height - y);
          surface = cairo_image_surface_create_for_data ((guchar *) data + stride * y + 4 * x,
                                                         CAIRO_FORMAT_ARGB32,
                                                         tile_width, tile_height,
                                                         stride);

          cairo_set_source_surface (cr, surface, x, y);
          cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_PAD);
          cairo_rectangle (cr, x, y, tile_width, tile_height);
          cairo_fill (cr);

          cairo_surface_finish (surface);
          cairo_surface_destroy (surface);
        }
    }

  cairo_pop_group_to_source (cr);
  cairo_paint (cr);
}

static void
gsk_texture_node_draw (GskRenderNode *node,
                       cairo_t       *cr,
                       GskCairoData  *data)
{
  GskTextureNode *self = (GskTextureNode *) node;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;
  int width, height;

  width = gdk_texture_get_width (self->texture);
  height = gdk_texture_get_height (self->texture);
  if (width > MAX_CAIRO_IMAGE_WIDTH || height > MAX_CAIRO_IMAGE_HEIGHT)
    {
      gsk_texture_node_draw_oversized (node, cr, data->ccs);
      return;
    }

  surface = gdk_texture_download_surface (self->texture, data->ccs);
  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  cairo_matrix_init_scale (&matrix,
                           width / node->bounds.size.width,
                           height / node->bounds.size.height);
  cairo_matrix_translate (&matrix,
                          -node->bounds.origin.x,
                          -node->bounds.origin.y);
  cairo_pattern_set_matrix (pattern, &matrix);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);

  gdk_cairo_rect (cr, &node->bounds);
  cairo_fill (cr);
}

static void
gsk_texture_node_diff (GskRenderNode *node1,
                       GskRenderNode *node2,
                       GskDiffData   *data)
{
  GskTextureNode *self1 = (GskTextureNode *) node1;
  GskTextureNode *self2 = (GskTextureNode *) node2;
  cairo_region_t *sub;

  if (!gsk_rect_equal (&node1->bounds, &node2->bounds) ||
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
gsk_texture_node_replay (GskRenderNode   *node,
                         GskRenderReplay *replay)
{
  GskTextureNode *self = (GskTextureNode *) node;
  GdkTexture *texture;
  GskRenderNode *result;

  texture = gsk_render_replay_filter_texture (replay, self->texture);
  if (self->texture == texture)
    {
      g_object_unref (texture);
      return gsk_render_node_ref (node);
    }

  result = gsk_texture_node_new (texture, &node->bounds);
  g_object_unref (texture);

  return result;
}

static void
gsk_texture_node_class_init (gpointer g_class,
                             gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_TEXTURE_NODE;

  node_class->finalize = gsk_texture_node_finalize;
  node_class->draw = gsk_texture_node_draw;
  node_class->diff = gsk_texture_node_diff;
  node_class->replay = gsk_texture_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskTextureNode, gsk_texture_node)

/**
 * gsk_texture_node_get_texture:
 * @node: (type GskTextureNode): a `GskRenderNode` of type %GSK_TEXTURE_NODE
 *
 * Retrieves the `GdkTexture` used when creating this `GskRenderNode`.
 *
 * Returns: (transfer none): the `GdkTexture`
 */
GdkTexture *
gsk_texture_node_get_texture (const GskRenderNode *node)
{
  const GskTextureNode *self = (const GskTextureNode *) node;

  return self->texture;
}

/**
 * gsk_texture_node_new:
 * @texture: the `GdkTexture`
 * @bounds: the rectangle to render the texture into
 *
 * Creates a `GskRenderNode` that will render the given
 * @texture into the area given by @bounds.
 *
 * Note that GSK applies linear filtering when textures are
 * scaled and transformed. See [class@Gsk.TextureScaleNode]
 * for a way to influence filtering.
 *
 * Returns: (transfer full) (type GskTextureNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_texture_node_new (GdkTexture            *texture,
                      const graphene_rect_t *bounds)
{
  GskTextureNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_TEXTURE_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = gdk_memory_format_alpha (gdk_texture_get_format (texture)) == GDK_MEMORY_ALPHA_OPAQUE;
  node->is_hdr = gdk_color_state_is_hdr (gdk_texture_get_color_state (texture));

  self->texture = g_object_ref (texture);
  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);

  node->preferred_depth = gdk_texture_get_depth (texture);

  return node;
}

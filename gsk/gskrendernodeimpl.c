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

#include "gskrendernodeprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gsktexture.h"

/*** GSK_TEXTURE_NODE ***/

static void
gsk_texture_node_finalize (GskRenderNode *node)
{
  gsk_texture_unref (node->texture);
}

static const GskRenderNodeClass GSK_TEXTURE_NODE_CLASS = {
  GSK_TEXTURE_NODE,
  "GskTextureNode",
  gsk_texture_node_finalize
};

GskTexture *
gsk_texture_node_get_texture (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TEXTURE_NODE), 0);

  return node->texture;
}

/**
 * gsk_texture_node_new:
 * @texture: the #GskTexture
 * @bounds: the rectangle to render the texture into
 *
 * Creates a #GskRenderNode that will render the given
 * @texture into the area given by @bounds.
 *
 * Returns: A new #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_texture_node_new (GskTexture            *texture,
                      const graphene_rect_t *bounds)
{
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_TEXTURE (texture), NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  node = gsk_render_node_new (&GSK_TEXTURE_NODE_CLASS);

  node->texture = gsk_texture_ref (texture);
  graphene_rect_init_from_rect (&node->bounds, bounds);

  return node;
}

/*** GSK_CAIRO_NODE ***/

static void
gsk_cairo_node_finalize (GskRenderNode *node)
{
  if (node->surface)
    cairo_surface_destroy (node->surface);
}

static const GskRenderNodeClass GSK_CAIRO_NODE_CLASS = {
  GSK_CAIRO_NODE,
  "GskCairoNode",
  gsk_cairo_node_finalize
};

/*< private >
 * gsk_cairo_node_get_surface:
 * @node: a #GskRenderNode
 *
 * Retrieves the surface set using gsk_render_node_set_surface().
 *
 * Returns: (transfer none) (nullable): a Cairo surface
 */
cairo_surface_t *
gsk_cairo_node_get_surface (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CAIRO_NODE), NULL);

  return node->surface;
}

/**
 * gsk_cairo_node_new:
 * @bounds: the rectangle to render the to
 *
 * Creates a #GskRenderNode that will render a cairo surface
 * into the area given by @bounds. You can draw to the cairo
 * surface using gsk_cairo_node_get_draw_context()
 *
 * Returns: A new #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_cairo_node_new (const graphene_rect_t *bounds)
{
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);

  node = gsk_render_node_new (&GSK_CAIRO_NODE_CLASS);

  graphene_rect_init_from_rect (&node->bounds, bounds);

  return node;
}

/**
 * gsk_cairo_node_get_draw_context:
 * @node: a cairo #GskRenderNode
 * @renderer: (nullable): Renderer to optimize for or %NULL for any
 *
 * Creates a Cairo context for drawing using the surface associated
 * to the render node.
 * If no surface exists yet, a surface will be created optimized for
 * rendering to @renderer.
 *
 * Returns: (transfer full): a Cairo context used for drawing; use
 *   cairo_destroy() when done drawing
 *
 * Since: 3.90
 */
cairo_t *
gsk_cairo_node_get_draw_context (GskRenderNode *node,
                                 GskRenderer   *renderer)
{
  int width, height;
  cairo_t *res;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CAIRO_NODE), NULL);
  g_return_val_if_fail (node->is_mutable, NULL);
  g_return_val_if_fail (renderer == NULL || GSK_IS_RENDERER (renderer), NULL);

  width = ceilf (node->bounds.size.width);
  height = ceilf (node->bounds.size.height);

  if (width <= 0 || height <= 0)
    {
      cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0);
      res = cairo_create (surface);
      cairo_surface_destroy (surface);
    }
  else if (node->surface == NULL)
    {
      if (renderer)
        {
          node->surface = gsk_renderer_create_cairo_surface (renderer,
                                                             node->opaque ? CAIRO_FORMAT_RGB24
                                                                          : CAIRO_FORMAT_ARGB32,
                                                             ceilf (node->bounds.size.width),
                                                             ceilf (node->bounds.size.height));
        }
      else
        {
          node->surface = cairo_image_surface_create (node->opaque ? CAIRO_FORMAT_RGB24
                                                                   : CAIRO_FORMAT_ARGB32,
                                                      ceilf (node->bounds.size.width),
                                                      ceilf (node->bounds.size.height));
        }
      res = cairo_create (node->surface);
    }
  else
    {
      res = cairo_create (node->surface);
    }

  cairo_translate (res, -node->bounds.origin.x, -node->bounds.origin.y);

  cairo_rectangle (res,
                   node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_clip (res);

  if (GSK_DEBUG_CHECK (SURFACE))
    {
      const char *prefix;
      prefix = g_getenv ("GSK_DEBUG_PREFIX");
      if (!prefix || g_str_has_prefix (node->name, prefix))
        {
          cairo_save (res);
          cairo_rectangle (res,
                           node->bounds.origin.x + 1, node->bounds.origin.y + 1,
                           node->bounds.size.width - 2, node->bounds.size.height - 2);
          cairo_set_line_width (res, 2);
          cairo_set_source_rgb (res, 1, 0, 0);
          cairo_stroke (res);
          cairo_restore (res);
        }
    }

  return res;
}

/**** GSK_CONTAINER_NODE ***/

static void
gsk_container_node_finalize (GskRenderNode *node)
{
}

static const GskRenderNodeClass GSK_CONTAINER_NODE_CLASS = {
  GSK_CONTAINER_NODE,
  "GskContainerNode",
  gsk_container_node_finalize
};

/**
 * gsk_container_node_new:
 *
 * Creates a new #GskRenderNode instance for holding multiple different
 * render nodes. You can use gsk_container_node_append_child() to add
 * nodes to the container.
 *
 * Returns: (transfer full): the new #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_container_node_new (void)
{
  return gsk_render_node_new (&GSK_CONTAINER_NODE_CLASS);
}


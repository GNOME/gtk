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

typedef struct _GskTextureNode GskTextureNode;

struct _GskTextureNode
{
  GskRenderNode render_node;

  GskTexture *texture;
  graphene_rect_t bounds;
};

static void
gsk_texture_node_finalize (GskRenderNode *node)
{
  GskTextureNode *self = (GskTextureNode *) node;

  gsk_texture_unref (self->texture);
}

static void
gsk_texture_node_make_immutable (GskRenderNode *node)
{
}

static void
gsk_texture_node_get_bounds (GskRenderNode   *node,
                             graphene_rect_t *bounds)
{
  GskTextureNode *self = (GskTextureNode *) node;

  graphene_rect_init_from_rect (bounds, &self->bounds); 
}

static const GskRenderNodeClass GSK_TEXTURE_NODE_CLASS = {
  GSK_TEXTURE_NODE,
  sizeof (GskTextureNode),
  "GskTextureNode",
  gsk_texture_node_finalize,
  gsk_texture_node_make_immutable,
  gsk_texture_node_get_bounds
};

GskTexture *
gsk_texture_node_get_texture (GskRenderNode *node)
{
  GskTextureNode *self = (GskTextureNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TEXTURE_NODE), 0);

  return self->texture;
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
  GskTextureNode *self;

  g_return_val_if_fail (GSK_IS_TEXTURE (texture), NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  self = (GskTextureNode *) gsk_render_node_new (&GSK_TEXTURE_NODE_CLASS);

  self->texture = gsk_texture_ref (texture);
  graphene_rect_init_from_rect (&self->bounds, bounds);

  return &self->render_node;
}

/*** GSK_CAIRO_NODE ***/

typedef struct _GskCairoNode GskCairoNode;

struct _GskCairoNode
{
  GskRenderNode render_node;

  cairo_surface_t *surface;
  graphene_rect_t bounds;
};

static void
gsk_cairo_node_finalize (GskRenderNode *node)
{
  GskCairoNode *self = (GskCairoNode *) node;

  if (self->surface)
    cairo_surface_destroy (self->surface);
}

static void
gsk_cairo_node_make_immutable (GskRenderNode *node)
{
}

static void
gsk_cairo_node_get_bounds (GskRenderNode   *node,
                           graphene_rect_t *bounds)
{
  GskCairoNode *self = (GskCairoNode *) node;

  graphene_rect_init_from_rect (bounds, &self->bounds); 
}

static const GskRenderNodeClass GSK_CAIRO_NODE_CLASS = {
  GSK_CAIRO_NODE,
  sizeof (GskCairoNode),
  "GskCairoNode",
  gsk_cairo_node_finalize,
  gsk_cairo_node_make_immutable,
  gsk_cairo_node_get_bounds
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
  GskCairoNode *self = (GskCairoNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CAIRO_NODE), NULL);

  return self->surface;
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
  GskCairoNode *self;

  g_return_val_if_fail (bounds != NULL, NULL);

  self = (GskCairoNode *) gsk_render_node_new (&GSK_CAIRO_NODE_CLASS);

  graphene_rect_init_from_rect (&self->bounds, bounds);

  return &self->render_node;
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
  GskCairoNode *self = (GskCairoNode *) node;
  int width, height;
  cairo_t *res;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CAIRO_NODE), NULL);
  g_return_val_if_fail (node->is_mutable, NULL);
  g_return_val_if_fail (renderer == NULL || GSK_IS_RENDERER (renderer), NULL);

  width = ceilf (self->bounds.size.width);
  height = ceilf (self->bounds.size.height);

  if (width <= 0 || height <= 0)
    {
      cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0);
      res = cairo_create (surface);
      cairo_surface_destroy (surface);
    }
  else if (self->surface == NULL)
    {
      if (renderer)
        {
          self->surface = gsk_renderer_create_cairo_surface (renderer,
                                                             CAIRO_FORMAT_ARGB32,
                                                             ceilf (self->bounds.size.width),
                                                             ceilf (self->bounds.size.height));
        }
      else
        {
          self->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                      ceilf (self->bounds.size.width),
                                                      ceilf (self->bounds.size.height));
        }
      res = cairo_create (self->surface);
    }
  else
    {
      res = cairo_create (self->surface);
    }

  cairo_translate (res, -self->bounds.origin.x, -self->bounds.origin.y);

  cairo_rectangle (res,
                   self->bounds.origin.x, self->bounds.origin.y,
                   self->bounds.size.width, self->bounds.size.height);
  cairo_clip (res);

  if (GSK_DEBUG_CHECK (SURFACE))
    {
      const char *prefix;
      prefix = g_getenv ("GSK_DEBUG_PREFIX");
      if (!prefix || g_str_has_prefix (node->name, prefix))
        {
          cairo_save (res);
          cairo_rectangle (res,
                           self->bounds.origin.x + 1, self->bounds.origin.y + 1,
                           self->bounds.size.width - 2, self->bounds.size.height - 2);
          cairo_set_line_width (res, 2);
          cairo_set_source_rgb (res, 1, 0, 0);
          cairo_stroke (res);
          cairo_restore (res);
        }
    }

  return res;
}

/**** GSK_CONTAINER_NODE ***/

typedef struct _GskContainerNode GskContainerNode;

struct _GskContainerNode
{
  GskRenderNode render_node;

  GPtrArray *children;
};

static void
gsk_container_node_finalize (GskRenderNode *node)
{
  GskContainerNode *container = (GskContainerNode *) node;

  g_ptr_array_unref (container->children);
}

static void
gsk_container_node_make_immutable (GskRenderNode *node)
{
  GskContainerNode *container = (GskContainerNode *) node;
  guint i;

  for (i = 1; i < container->children->len; i++)
    {
      gsk_render_node_make_immutable (g_ptr_array_index (container->children, i));
    }
}

static void
gsk_container_node_get_bounds (GskRenderNode   *node,
                               graphene_rect_t *bounds)
{
  GskContainerNode *container = (GskContainerNode *) node;
  guint i;

  if (container->n_children == 0)
    {
      graphene_rect_init_from_rect (bounds, graphene_rect_zero()); 
      return;
    }

  gsk_render_node_get_bounds (container->children[0], bounds);

  for (i = 1; i < container->n_children; i++)
    {
      graphene_rect_t child_bounds;
  
      gsk_render_node_get_bounds (container->children[i], &child_bounds);
      graphene_rect_union (bounds, &child_bounds, bounds);
    }
}

static const GskRenderNodeClass GSK_CONTAINER_NODE_CLASS = {
  GSK_CONTAINER_NODE,
  sizeof (GskContainerNode),
  "GskContainerNode",
  gsk_container_node_finalize,
  gsk_container_node_make_immutable,
  gsk_container_node_get_bounds
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
  GskContainerNode *container;

  container = (GskContainerNode *) gsk_render_node_new (&GSK_CONTAINER_NODE_CLASS);

  container->children = g_ptr_array_new_with_free_func ((GDestroyNotify) gsk_render_node_unref);

  return &container->render_node;
}

/**
 * gsk_container_node_append_child:
 * @node: a container node
 * @child: a #GskRenderNode
 *
 * Appends @child to the list of children of @node.
 *
 * This function acquires a reference on @child.
 *
 * Returns: (transfer none): the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_container_node_append_child (GskRenderNode *node,
                                 GskRenderNode *child)
{
  GskContainerNode *container = (GskContainerNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), node);
  g_return_val_if_fail (node->is_mutable, node);

  g_ptr_array_add (container->children, gsk_render_node_ref (child));

  return node;
}

/**
 * gsk_container_node_get_n_children:
 * @node: a container #GskRenderNode
 *
 * Retrieves the number of direct children of @node.
 *
 * Returns: the number of children of the #GskRenderNode
 *
 * Since: 3.90
 */
guint
gsk_container_node_get_n_children (GskRenderNode *node)
{
  GskContainerNode *container = (GskContainerNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE), 0);

  return container->children->len;
}

GskRenderNode *
gsk_container_node_get_child (GskRenderNode *node,
                              guint          idx)
{
  GskContainerNode *container = (GskContainerNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE), NULL);
  g_return_val_if_fail (idx < container->children->len, 0);

  return g_ptr_array_index (container->children, idx);
}


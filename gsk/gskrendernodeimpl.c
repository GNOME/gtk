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
#include "gsktextureprivate.h"

/*** GSK_COLOR_NODE ***/

typedef struct _GskColorNode GskColorNode;

struct _GskColorNode
{
  GskRenderNode render_node;

  GdkRGBA color;
  graphene_rect_t bounds;
};

static void
gsk_color_node_finalize (GskRenderNode *node)
{
}

static void
gsk_color_node_make_immutable (GskRenderNode *node)
{
}

static void
gsk_color_node_draw (GskRenderNode *node,
                     cairo_t       *cr)
{
  GskColorNode *self = (GskColorNode *) node;

  gdk_cairo_set_source_rgba (cr, &self->color);

  cairo_rectangle (cr,
                   self->bounds.origin.x, self->bounds.origin.y,
                   self->bounds.size.width, self->bounds.size.height);
  cairo_fill (cr);
}

static void
gsk_color_node_get_bounds (GskRenderNode   *node,
                           graphene_rect_t *bounds)
{
  GskColorNode *self = (GskColorNode *) node;

  graphene_rect_init_from_rect (bounds, &self->bounds); 
}

static const GskRenderNodeClass GSK_COLOR_NODE_CLASS = {
  GSK_COLOR_NODE,
  sizeof (GskColorNode),
  "GskColorNode",
  gsk_color_node_finalize,
  gsk_color_node_make_immutable,
  gsk_color_node_draw,
  gsk_color_node_get_bounds
};

const GdkRGBA *
gsk_color_node_peek_color (GskRenderNode *node)
{
  GskColorNode *self = (GskColorNode *) node;

  return &self->color;
}

/**
 * gsk_color_node_new:
 * @color: the #GskColor
 * @bounds: the rectangle to render the color into
 *
 * Creates a #GskRenderNode that will render the given
 * @color into the area given by @bounds.
 *
 * Returns: A new #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_color_node_new (const GdkRGBA         *rgba,
                    const graphene_rect_t *bounds)
{
  GskColorNode *self;

  g_return_val_if_fail (rgba != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  self = (GskColorNode *) gsk_render_node_new (&GSK_COLOR_NODE_CLASS);

  self->color = *rgba;
  graphene_rect_init_from_rect (&self->bounds, bounds);

  return &self->render_node;
}

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
gsk_texture_node_draw (GskRenderNode *node,
                       cairo_t       *cr)
{
  GskTextureNode *self = (GskTextureNode *) node;
  cairo_surface_t *surface;

  surface = gsk_texture_download (self->texture);

  cairo_save (cr);

  cairo_translate (cr, self->bounds.origin.x, self->bounds.origin.y);
  cairo_scale (cr,
               self->bounds.size.width / gsk_texture_get_width (self->texture),
               self->bounds.size.height / gsk_texture_get_height (self->texture));

  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  cairo_restore (cr);

  cairo_surface_destroy (surface);
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
  gsk_texture_node_draw,
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
gsk_cairo_node_draw (GskRenderNode *node,
                     cairo_t       *cr)
{
  GskCairoNode *self = (GskCairoNode *) node;

  if (self->surface == NULL)
    return;

  cairo_set_source_surface (cr, self->surface, self->bounds.origin.x, self->bounds.origin.y);
  cairo_paint (cr);
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
  gsk_cairo_node_draw,
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

  GskRenderNode **children;
  guint n_children;
};

static void
gsk_container_node_finalize (GskRenderNode *node)
{
  GskContainerNode *container = (GskContainerNode *) node;
  guint i;

  for (i = 0; i < container->n_children; i++)
    gsk_render_node_unref (container->children[i]);

  g_free (container->children);
}

static void
gsk_container_node_make_immutable (GskRenderNode *node)
{
  GskContainerNode *container = (GskContainerNode *) node;
  guint i;

  for (i = 1; i < container->n_children; i++)
    {
      gsk_render_node_make_immutable (container->children[i]);
    }
}

static void
gsk_container_node_draw (GskRenderNode *node,
                         cairo_t       *cr)
{
  GskContainerNode *container = (GskContainerNode *) node;
  guint i;

  for (i = 0; i < container->n_children; i++)
    {
      gsk_render_node_draw (container->children[i], cr);
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
  gsk_container_node_draw,
  gsk_container_node_get_bounds
};

/**
 * gsk_container_node_new:
 * @children: (array length=n_children) (transfer none): The children of the node
 * @n_children: Number of children in the @children array
 *
 * Creates a new #GskRenderNode instance for holding the given @children.
 * The new node will acquire a reference to each of the children.
 *
 * Returns: (transfer full): the new #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_container_node_new (GskRenderNode **children,
                        guint           n_children)
{
  GskContainerNode *container;
  guint i;

  container = (GskContainerNode *) gsk_render_node_new (&GSK_CONTAINER_NODE_CLASS);

  container->children = g_memdup (children, sizeof (GskRenderNode *) * n_children);
  container->n_children = n_children;

  for (i = 0; i < container->n_children; i++)
    gsk_render_node_ref (container->children[i]);

  return &container->render_node;
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

  return container->n_children;
}

GskRenderNode *
gsk_container_node_get_child (GskRenderNode *node,
                              guint          idx)
{
  GskContainerNode *container = (GskContainerNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE), NULL);
  g_return_val_if_fail (idx < container->n_children, 0);

  return container->children[idx];
}

/*** GSK_TRANSFORM_NODE ***/

typedef struct _GskTransformNode GskTransformNode;

struct _GskTransformNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  graphene_matrix_t transform;
};

static void
gsk_transform_node_finalize (GskRenderNode *node)
{
  GskTransformNode *self = (GskTransformNode *) node;

  gsk_render_node_unref (self->child);
}

static void
gsk_transform_node_make_immutable (GskRenderNode *node)
{
  GskTransformNode *self = (GskTransformNode *) node;

  gsk_render_node_make_immutable (self->child);
}

static void
gsk_transform_node_draw (GskRenderNode *node,
                         cairo_t       *cr)
{
  GskTransformNode *self = (GskTransformNode *) node;
  cairo_matrix_t ctm;

  if (graphene_matrix_to_2d (&self->transform, &ctm.xx, &ctm.yx, &ctm.xy, &ctm.yy, &ctm.x0, &ctm.y0))
    {
      GSK_NOTE (CAIRO, g_print ("CTM = { .xx = %g, .yx = %g, .xy = %g, .yy = %g, .x0 = %g, .y0 = %g }\n",
                                ctm.xx, ctm.yx,
                                ctm.xy, ctm.yy,
                                ctm.x0, ctm.y0));
      cairo_transform (cr, &ctm);
  
      gsk_render_node_draw (self->child, cr);
    }
  else
    {
      graphene_rect_t bounds;

      gsk_render_node_get_bounds (node, &bounds);

      cairo_set_source_rgb (cr, 255 / 255., 105 / 255., 180 / 255.);
      cairo_rectangle (cr, bounds.origin.x, bounds.origin.x, bounds.size.width, bounds.size.height);
      cairo_fill (cr);
    }
}

static void
gsk_transform_node_get_bounds (GskRenderNode   *node,
                               graphene_rect_t *bounds)
{
  GskTransformNode *self = (GskTransformNode *) node;
  graphene_rect_t child_bounds;

  gsk_render_node_get_bounds (self->child, &child_bounds);

  graphene_matrix_transform_bounds (&self->transform,
                                    &child_bounds,
                                    bounds);
}

static const GskRenderNodeClass GSK_TRANSFORM_NODE_CLASS = {
  GSK_TRANSFORM_NODE,
  sizeof (GskTransformNode),
  "GskTransformNode",
  gsk_transform_node_finalize,
  gsk_transform_node_make_immutable,
  gsk_transform_node_draw,
  gsk_transform_node_get_bounds
};

/**
 * gsk_transform_node_new:
 * @child: The node to transform
 * @transform: The transform to apply
 *
 * Creates a #GskRenderNode that will transform the given @child
 * with the given @transform.
 *
 * Returns: A new #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_transform_node_new (GskRenderNode           *child,
                        const graphene_matrix_t *transform)
{
  GskTransformNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (transform != NULL, NULL);

  self = (GskTransformNode *) gsk_render_node_new (&GSK_TRANSFORM_NODE_CLASS);

  self->child = gsk_render_node_ref (child);
  graphene_matrix_init_from_matrix (&self->transform, transform);

  return &self->render_node;
}

/**
 * gsk_transform_node_get_child:
 * @node: a transform @GskRenderNode
 *
 * Gets the child node that is getting transformed by the given @node.
 *
 * Returns: (transfer none): The child that is getting transformed
 **/
GskRenderNode *
gsk_transform_node_get_child (GskRenderNode *node)
{
  GskTransformNode *self = (GskTransformNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TRANSFORM_NODE), NULL);

  return self->child;
}

void
gsk_transform_node_get_transform (GskRenderNode     *node,
                                  graphene_matrix_t *transform)
{
  GskTransformNode *self = (GskTransformNode *) node;

  g_return_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TRANSFORM_NODE));

  graphene_matrix_init_from_matrix (transform, &self->transform);
}

/*** GSK_OPACITY_NODE ***/

typedef struct _GskOpacityNode GskOpacityNode;

struct _GskOpacityNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  double opacity;
};

static void
gsk_opacity_node_finalize (GskRenderNode *node)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  gsk_render_node_unref (self->child);
}

static void
gsk_opacity_node_make_immutable (GskRenderNode *node)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  gsk_render_node_make_immutable (self->child);
}

static void
gsk_opacity_node_draw (GskRenderNode *node,
                       cairo_t       *cr)
{
  GskOpacityNode *self = (GskOpacityNode *) node;
  graphene_rect_t bounds;

  cairo_save (cr);

  /* clip so the push_group() creates a smaller surface */
  gsk_render_node_get_bounds (self->child, &bounds);
  cairo_rectangle (cr, bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);
  cairo_clip (cr);

  cairo_push_group (cr);

  gsk_render_node_draw (self->child, cr);

  cairo_pop_group_to_source (cr);
  cairo_paint_with_alpha (cr, self->opacity);

  cairo_restore (cr);
}

static void
gsk_opacity_node_get_bounds (GskRenderNode   *node,
                             graphene_rect_t *bounds)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  gsk_render_node_get_bounds (self->child, bounds);
}

static const GskRenderNodeClass GSK_OPACITY_NODE_CLASS = {
  GSK_OPACITY_NODE,
  sizeof (GskOpacityNode),
  "GskOpacityNode",
  gsk_opacity_node_finalize,
  gsk_opacity_node_make_immutable,
  gsk_opacity_node_draw,
  gsk_opacity_node_get_bounds
};

/**
 * gsk_opacity_node_new:
 * @child: The node to draw
 * @opacity: The opacity to apply
 *
 * Creates a #GskRenderNode that will drawn the @child with reduced
 * @opacity.
 *
 * Returns: A new #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_opacity_node_new (GskRenderNode *child,
                      double         opacity)
{
  GskOpacityNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = (GskOpacityNode *) gsk_render_node_new (&GSK_OPACITY_NODE_CLASS);

  self->child = gsk_render_node_ref (child);
  self->opacity = CLAMP (opacity, 0.0, 1.0);

  return &self->render_node;
}

/**
 * gsk_opacity_node_get_child:
 * @node: a opacity @GskRenderNode
 *
 * Gets the child node that is getting opacityed by the given @node.
 *
 * Returns: (transfer none): The child that is getting opacityed
 **/
GskRenderNode *
gsk_opacity_node_get_child (GskRenderNode *node)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OPACITY_NODE), NULL);

  return self->child;
}

double
gsk_opacity_node_get_opacity (GskRenderNode *node)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OPACITY_NODE), 1.0);

  return self->opacity;
}

/*** GSK_CLIP_NODE ***/

typedef struct _GskClipNode GskClipNode;

struct _GskClipNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  graphene_rect_t clip;
};

static void
gsk_clip_node_finalize (GskRenderNode *node)
{
  GskClipNode *self = (GskClipNode *) node;

  gsk_render_node_unref (self->child);
}

static void
gsk_clip_node_make_immutable (GskRenderNode *node)
{
  GskClipNode *self = (GskClipNode *) node;

  gsk_render_node_make_immutable (self->child);
}

static void
gsk_clip_node_draw (GskRenderNode *node,
                    cairo_t       *cr)
{
  GskClipNode *self = (GskClipNode *) node;

  cairo_save (cr);

  cairo_rectangle (cr,
                   self->clip.origin.x, self->clip.origin.y,
                   self->clip.size.width, self->clip.size.height);
  cairo_clip (cr);

  gsk_render_node_draw (self->child, cr);

  cairo_restore (cr);
}

static void
gsk_clip_node_get_bounds (GskRenderNode   *node,
                          graphene_rect_t *bounds)
{
  GskClipNode *self = (GskClipNode *) node;
  graphene_rect_t child_bounds;

  gsk_render_node_get_bounds (self->child, &child_bounds);

  graphene_rect_intersection (&self->clip, &child_bounds, bounds);
}

static const GskRenderNodeClass GSK_CLIP_NODE_CLASS = {
  GSK_CLIP_NODE,
  sizeof (GskClipNode),
  "GskClipNode",
  gsk_clip_node_finalize,
  gsk_clip_node_make_immutable,
  gsk_clip_node_draw,
  gsk_clip_node_get_bounds
};

/**
 * gsk_clip_node_new:
 * @child: The node to draw
 * @clip: The clip to apply
 *
 * Creates a #GskRenderNode that will clip the @child to the area
 * given by @clip.
 *
 * Returns: A new #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_clip_node_new (GskRenderNode         *child,
                   const graphene_rect_t *clip)
{
  GskClipNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (clip != NULL, NULL);

  self = (GskClipNode *) gsk_render_node_new (&GSK_CLIP_NODE_CLASS);

  self->child = gsk_render_node_ref (child);
  graphene_rect_normalize_r (clip, &self->clip);

  return &self->render_node;
}

/**
 * gsk_clip_node_get_child:
 * @node: a clip @GskRenderNode
 *
 * Gets the child node that is getting clipped by the given @node.
 *
 * Returns: (transfer none): The child that is getting clipped
 **/
GskRenderNode *
gsk_clip_node_get_child (GskRenderNode *node)
{
  GskClipNode *self = (GskClipNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CLIP_NODE), NULL);

  return self->child;
}

const graphene_rect_t *
gsk_clip_node_peek_clip (GskRenderNode *node)
{
  GskClipNode *self = (GskClipNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CLIP_NODE), NULL);

  return &self->clip;
}


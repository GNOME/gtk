#include "config.h"

#include "gskcairorendererprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeiter.h"
#include "gskrendernodeprivate.h"
#include "gsktextureprivate.h"

struct _GskCairoRenderer
{
  GskRenderer parent_instance;

  graphene_rect_t viewport;
};

struct _GskCairoRendererClass
{
  GskRendererClass parent_class;
};

typedef struct _GskCairoTexture GskCairoTexture;
struct _GskCairoTexture {
  GskTexture texture;
  cairo_surface_t *surface;
};

G_DEFINE_TYPE (GskCairoRenderer, gsk_cairo_renderer, GSK_TYPE_RENDERER)

static gboolean
gsk_cairo_renderer_realize (GskRenderer *renderer)
{
  return TRUE;
}

static void
gsk_cairo_renderer_unrealize (GskRenderer *renderer)
{

}

static void
gsk_cairo_renderer_render_node (GskCairoRenderer *self,
                                GskRenderNode    *node,
                                cairo_t          *cr)
{
  GskRenderNodeIter iter;
  GskRenderNode *child;
  gboolean pop_group = FALSE;
  graphene_matrix_t mvp;
  cairo_matrix_t ctm;
  graphene_rect_t frame;

  if (gsk_render_node_is_hidden (node))
    return;

  cairo_save (cr);

  if (!gsk_render_node_has_surface (node) &&
      !gsk_render_node_has_texture (node))
    goto out;

  gsk_render_node_get_world_matrix (node, &mvp);
  if (graphene_matrix_to_2d (&mvp, &ctm.xx, &ctm.yx, &ctm.xy, &ctm.yy, &ctm.x0, &ctm.y0))
    {
      GSK_NOTE (CAIRO, g_print ("CTM = { .xx = %g, .yx = %g, .xy = %g, .yy = %g, .x0 = %g, .y0 = %g }\n",
                                ctm.xx, ctm.yx,
                                ctm.xy, ctm.yy,
                                ctm.x0, ctm.y0));
      cairo_transform (cr, &ctm);
    }
  else
    g_critical ("Invalid non-affine transformation for node %p", node);

  gsk_render_node_get_bounds (node, &frame);
  GSK_NOTE (CAIRO, g_print ("CLIP = { .x = %g, .y = %g, .width = %g, .height = %g }\n",
                            frame.origin.x, frame.origin.y,
                            frame.size.width, frame.size.height));

  if (!GSK_RENDER_MODE_CHECK (GEOMETRY))
    {
      cairo_rectangle (cr, frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
      cairo_clip (cr);
    }

  if (!gsk_render_node_is_opaque (node) && gsk_render_node_get_opacity (node) != 1.0)
    {
      GSK_NOTE (CAIRO, g_print ("Pushing opacity group (opacity:%g)\n",
                                gsk_render_node_get_opacity (node)));
      cairo_push_group (cr);
      pop_group = TRUE;
    }

  GSK_NOTE (CAIRO, g_print ("Rendering surface %p for node %s[%p] at %g, %g\n",
                            gsk_render_node_get_surface (node),
                            node->name,
                            node,
                            frame.origin.x, frame.origin.y));
  if (gsk_render_node_has_texture (node))
    {
      GskCairoTexture *cairo_texture = (GskCairoTexture *) gsk_render_node_get_texture (node);

      cairo_set_source_surface (cr, cairo_texture->surface, frame.origin.x, frame.origin.y); 
      cairo_paint (cr);
    }
  else
    {
      cairo_set_source_surface (cr, gsk_render_node_get_surface (node), frame.origin.x, frame.origin.y); 
      cairo_paint (cr);
    }

  if (GSK_RENDER_MODE_CHECK (GEOMETRY))
    {
      cairo_save (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_rectangle (cr, frame.origin.x - 1, frame.origin.y - 1, frame.size.width + 2, frame.size.height + 2);
      cairo_set_line_width (cr, 2);
      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  cairo_matrix_invert (&ctm);
  cairo_transform (cr, &ctm);

out:
  if (gsk_render_node_get_n_children (node) != 0)
    {
      GSK_NOTE (CAIRO, g_print ("Drawing %d children of node [%p]\n",
                                gsk_render_node_get_n_children (node),
                                node));
      gsk_render_node_iter_init (&iter, node);
      while (gsk_render_node_iter_next (&iter, &child))
        gsk_cairo_renderer_render_node (self, child, cr);
    }

  if (pop_group)
    {
      cairo_pop_group_to_source (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_paint_with_alpha (cr, gsk_render_node_get_opacity (node));
    }

  cairo_restore (cr);
}

static void
gsk_cairo_renderer_render (GskRenderer   *renderer,
                           GskRenderNode *root)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);
  GdkDrawingContext *context = gsk_renderer_get_drawing_context (renderer);
  cairo_t *cr;

  if (context != NULL)
    cr = gdk_drawing_context_get_cairo_context (context);
  else
    cr = gsk_renderer_get_cairo_context (renderer);

  if (cr == NULL)
    return;

  gsk_renderer_get_viewport (renderer, &self->viewport);

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);
  cairo_restore (cr);

  if (GSK_RENDER_MODE_CHECK (GEOMETRY))
    {
      cairo_save (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_rectangle (cr,
                       self->viewport.origin.x,
                       self->viewport.origin.y,
                       self->viewport.size.width,
                       self->viewport.size.height);
      cairo_set_source_rgba (cr, 0, 0, 0.85, 0.5);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  gsk_cairo_renderer_render_node (self, root, cr);
}

static GskTexture *
gsk_cairo_texture_new_for_surface (GskRenderer     *renderer,
                                   cairo_surface_t *surface,
                                   int              width,
                                   int              height)
{
  GskCairoTexture *texture;

  texture = gsk_texture_new (GskCairoTexture, renderer, width, height);

  texture->surface = cairo_surface_reference (surface);

  return (GskTexture *) texture;
}

static GskTexture *
gsk_cairo_renderer_texture_new_for_data (GskRenderer  *renderer,
                                         const guchar *data,
                                         int           width,
                                         int           height,
                                         int           stride)
{
  GskTexture *texture;
  cairo_surface_t *original, *copy;
  cairo_t *cr;

  original = cairo_image_surface_create_for_data ((guchar *) data, CAIRO_FORMAT_ARGB32, width, height, stride);
  copy = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  cr = cairo_create (copy);
  cairo_set_source_surface (cr, original, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  texture = gsk_cairo_texture_new_for_surface (renderer,
                                               copy,
                                               width, height);

  cairo_surface_destroy (copy);
  cairo_surface_destroy (original);

  return texture;
}

static GskTexture *
gsk_cairo_renderer_texture_new_for_pixbuf (GskRenderer *renderer,
                                           GdkPixbuf   *pixbuf)
{
  GskTexture *texture;
  cairo_surface_t *surface;

  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 1, NULL);

  texture = gsk_cairo_texture_new_for_surface (renderer,
                                               surface,
                                               gdk_pixbuf_get_width (pixbuf),
                                               gdk_pixbuf_get_height (pixbuf));

  cairo_surface_destroy (surface);

  return texture;
}

static void
gsk_cairo_renderer_texture_destroy (GskTexture *texture)
{
  GskCairoTexture *cairo_texture = (GskCairoTexture *) texture;

  cairo_surface_destroy (cairo_texture->surface);
}

static void
gsk_cairo_renderer_class_init (GskCairoRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_cairo_renderer_realize;
  renderer_class->unrealize = gsk_cairo_renderer_unrealize;
  renderer_class->render = gsk_cairo_renderer_render;

  renderer_class->texture_new_for_data = gsk_cairo_renderer_texture_new_for_data;
  renderer_class->texture_new_for_pixbuf = gsk_cairo_renderer_texture_new_for_pixbuf;
  renderer_class->texture_destroy = gsk_cairo_renderer_texture_destroy;
}

static void
gsk_cairo_renderer_init (GskCairoRenderer *self)
{

}

#include "config.h"

#include "gskcairorendererprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeiter.h"
#include "gskrendernodeprivate.h"

struct _GskCairoRenderer
{
  GskRenderer parent_instance;

  graphene_rect_t viewport;
};

struct _GskCairoRendererClass
{
  GskRendererClass parent_class;
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

  GSK_NOTE (CAIRO, g_print ("Rendering surface %p for node %p at %g, %g\n",
                            gsk_render_node_get_surface (node),
                            node,
                            frame.origin.x, frame.origin.y));
  cairo_set_source_surface (cr, gsk_render_node_get_surface (node), frame.origin.x, frame.origin.y); 
  cairo_paint (cr);

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
gsk_cairo_renderer_resize_viewport (GskRenderer           *renderer,
                                    const graphene_rect_t *viewport)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);

  self->viewport = *viewport;
}

static void
gsk_cairo_renderer_render (GskRenderer *renderer)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);
  cairo_surface_t *target = gsk_renderer_get_surface (renderer);
  GskRenderNode *root = gsk_renderer_get_root_node (renderer);
  cairo_t *cr = cairo_create (target);

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

  cairo_destroy (cr);
}

static void
gsk_cairo_renderer_clear (GskRenderer *renderer)
{
  cairo_surface_t *surface = gsk_renderer_get_surface (renderer);
  cairo_t *cr = cairo_create (surface);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  if (gsk_renderer_get_use_alpha (renderer))
    cairo_set_source_rgba (cr, 0, 0, 0, 0);
  else
    cairo_set_source_rgb (cr, 0, 0, 0);

  cairo_paint (cr);

  cairo_destroy (cr);
}

static void
gsk_cairo_renderer_class_init (GskCairoRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_cairo_renderer_realize;
  renderer_class->unrealize = gsk_cairo_renderer_unrealize;
  renderer_class->resize_viewport = gsk_cairo_renderer_resize_viewport;
  renderer_class->clear = gsk_cairo_renderer_clear;
  renderer_class->render = gsk_cairo_renderer_render;
}

static void
gsk_cairo_renderer_init (GskCairoRenderer *self)
{

}

#include "config.h"

#include "gskbroadwayrendererprivate.h"
#include "broadway/gdkprivate-broadway.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gdk/gdktextureprivate.h"


struct _GskBroadwayRenderer
{
  GskRenderer parent_instance;

};

struct _GskBroadwayRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskBroadwayRenderer, gsk_broadway_renderer, GSK_TYPE_RENDERER)

static gboolean
gsk_broadway_renderer_realize (GskRenderer  *renderer,
                               GdkWindow    *window,
                               GError      **error)
{
  return TRUE;
}

static void
gsk_broadway_renderer_unrealize (GskRenderer *renderer)
{

}

static GdkTexture *
gsk_broadway_renderer_render_texture (GskRenderer           *renderer,
                                      GskRenderNode         *root,
                                      const graphene_rect_t *viewport)
{
  GdkTexture *texture;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ceil (viewport->size.width), ceil (viewport->size.height));
  cr = cairo_create (surface);

  cairo_translate (cr, - viewport->origin.x, - viewport->origin.y);

  gsk_render_node_draw (root, cr);

  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static void
gsk_broadway_renderer_render (GskRenderer   *renderer,
                              GskRenderNode *root)
{
  GdkDrawingContext *context = gsk_renderer_get_drawing_context (renderer);
  graphene_rect_t viewport;
  cairo_t *cr;

  cr = gdk_drawing_context_get_cairo_context (context);

  g_return_if_fail (cr != NULL);

  gsk_renderer_get_viewport (renderer, &viewport);

  if (GSK_RENDER_MODE_CHECK (GEOMETRY))
    {
      cairo_save (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_rectangle (cr,
                       viewport.origin.x,
                       viewport.origin.y,
                       viewport.size.width,
                       viewport.size.height);
      cairo_set_source_rgba (cr, 0, 0, 0.85, 0.5);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  gsk_render_node_draw (root, cr);
}

static void
gsk_broadway_renderer_class_init (GskBroadwayRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_broadway_renderer_realize;
  renderer_class->unrealize = gsk_broadway_renderer_unrealize;
  renderer_class->render = gsk_broadway_renderer_render;
  renderer_class->render_texture = gsk_broadway_renderer_render_texture;
}

static void
gsk_broadway_renderer_init (GskBroadwayRenderer *self)
{
}

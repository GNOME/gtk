#include "config.h"

#include "gskcairorendererprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gsktextureprivate.h"

#ifdef G_ENABLE_DEBUG
typedef struct {
  GQuark cpu_time;
  GQuark gpu_time;
} ProfileTimers;
#endif

struct _GskCairoRenderer
{
  GskRenderer parent_instance;

#ifdef G_ENABLE_DEBUG
  ProfileTimers profile_timers;
#endif
};

struct _GskCairoRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskCairoRenderer, gsk_cairo_renderer, GSK_TYPE_RENDERER)

static gboolean
gsk_cairo_renderer_realize (GskRenderer  *renderer,
                            GdkWindow    *window,
                            GError      **error)
{
  return TRUE;
}

static void
gsk_cairo_renderer_unrealize (GskRenderer *renderer)
{

}

static void
gsk_cairo_renderer_do_render (GskRenderer   *renderer,
                              cairo_t       *cr,
                              GskRenderNode *root)
{
#ifdef G_ENABLE_DEBUG
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);
  GskProfiler *profiler;
  gint64 cpu_time;
#endif

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  gsk_render_node_draw (root, cr);

#ifdef G_ENABLE_DEBUG
  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gsk_profiler_push_samples (profiler);
#endif
}

static GskTexture *
gsk_cairo_renderer_render_texture (GskRenderer           *renderer,
                                   GskRenderNode         *root,
                                   const graphene_rect_t *viewport)
{
  GskTexture *texture;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ceil (viewport->size.width), ceil (viewport->size.height));
  cr = cairo_create (surface);

  cairo_translate (cr, - viewport->origin.x, - viewport->origin.y);

  gsk_cairo_renderer_do_render (renderer, cr, root);

  cairo_destroy (cr);

  texture = gsk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static void
gsk_cairo_renderer_render (GskRenderer   *renderer,
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

  gsk_cairo_renderer_do_render (renderer, cr, root);
}

static void
gsk_cairo_renderer_class_init (GskCairoRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_cairo_renderer_realize;
  renderer_class->unrealize = gsk_cairo_renderer_unrealize;
  renderer_class->render = gsk_cairo_renderer_render;
  renderer_class->render_texture = gsk_cairo_renderer_render_texture;
}

static void
gsk_cairo_renderer_init (GskCairoRenderer *self)
{
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler = gsk_renderer_get_profiler (GSK_RENDERER (self));

  self->profile_timers.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU time", FALSE, TRUE);
#endif
}

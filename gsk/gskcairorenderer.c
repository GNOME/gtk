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

  graphene_rect_t viewport;

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
gsk_cairo_renderer_render (GskRenderer   *renderer,
                           GskRenderNode *root)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);
  GdkDrawingContext *context = gsk_renderer_get_drawing_context (renderer);
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 cpu_time;
#endif

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

static void
gsk_cairo_renderer_class_init (GskCairoRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_cairo_renderer_realize;
  renderer_class->unrealize = gsk_cairo_renderer_unrealize;
  renderer_class->render = gsk_cairo_renderer_render;
}

static void
gsk_cairo_renderer_init (GskCairoRenderer *self)
{
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler = gsk_renderer_get_profiler (GSK_RENDERER (self));

  self->profile_timers.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU time", FALSE, TRUE);
#endif
}

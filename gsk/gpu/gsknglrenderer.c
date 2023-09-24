#include "config.h"

#include "gsknglrendererprivate.h"

#include "gskgpuimageprivate.h"
#include "gskgpurendererprivate.h"
#include "gskgldeviceprivate.h"
#include "gskglframeprivate.h"
#include "gskglimageprivate.h"

#include "gdk/gdkglcontextprivate.h"

struct _GskNglRenderer
{
  GskGpuRenderer parent_instance;

  GskGpuImage *backbuffer;
};

struct _GskNglRendererClass
{
  GskGpuRendererClass parent_class;
};

G_DEFINE_TYPE (GskNglRenderer, gsk_ngl_renderer, GSK_TYPE_GPU_RENDERER)

static GdkDrawContext *
gsk_ngl_renderer_create_context (GskGpuRenderer       *renderer,
                                 GdkDisplay           *display,
                                 GdkSurface           *surface,
                                 GskGpuOptimizations  *supported,
                                 GError              **error)
{
  GdkGLContext *context;

  if (surface)
    context = gdk_surface_create_gl_context (surface, error);
  else
    context = gdk_display_create_gl_context (display, error);

  if (context == NULL)
    return NULL;

  /* GLES 2 is not supported */
  gdk_gl_context_set_required_version (context, 3, 0);

  if (!gdk_gl_context_realize (context, error))
    {
      g_object_unref (context);
      return NULL;
    }

  gdk_gl_context_make_current (context);

  *supported = -1;
  if (!gdk_gl_context_check_version (context, "4.2", "9.9") &&
      !epoxy_has_gl_extension ("GL_EXT_base_instance") &&
      !epoxy_has_gl_extension ("GL_ARB_base_instance"))
    *supported &= ~GSK_GPU_OPTIMIZE_GL_BASE_INSTANCE;

  return GDK_DRAW_CONTEXT (context);
}

static void
gsk_ngl_renderer_make_current (GskGpuRenderer *renderer)
{
  gdk_gl_context_make_current (GDK_GL_CONTEXT (gsk_gpu_renderer_get_context (renderer)));
}

static GskGpuImage *
gsk_ngl_renderer_get_backbuffer (GskGpuRenderer *renderer)
{
  GskNglRenderer *self = GSK_NGL_RENDERER (renderer);
  GdkDrawContext *context;
  GdkSurface *surface;
  float scale;

  context = gsk_gpu_renderer_get_context (renderer);
  surface = gdk_draw_context_get_surface (context);
  scale = gdk_surface_get_scale (surface);

  if (self->backbuffer == NULL ||
      gsk_gpu_image_get_width (self->backbuffer) != ceil (gdk_surface_get_width (surface) * scale) ||
      gsk_gpu_image_get_height (self->backbuffer) != ceil (gdk_surface_get_height (surface) * scale))
    {
      g_clear_object (&self->backbuffer);
      self->backbuffer = gsk_gl_image_new_backbuffer (GSK_GL_DEVICE (gsk_gpu_renderer_get_device (renderer)),
                                                      GDK_MEMORY_DEFAULT /* FIXME */,
                                                      ceil (gdk_surface_get_width (surface) * scale),
                                                      ceil (gdk_surface_get_height (surface) * scale));
    }

  return self->backbuffer;
}

static void
gsk_ngl_renderer_wait (GskGpuRenderer  *self,
                       GskGpuFrame    **frame,
                       gsize            n_frames)
{
}

static void
gsk_ngl_renderer_class_init (GskNglRendererClass *klass)
{
  GskGpuRendererClass *gpu_renderer_class = GSK_GPU_RENDERER_CLASS (klass);

  gpu_renderer_class->frame_type = GSK_TYPE_GL_FRAME;

  gpu_renderer_class->get_device = gsk_gl_device_get_for_display;
  gpu_renderer_class->create_context = gsk_ngl_renderer_create_context;
  gpu_renderer_class->make_current = gsk_ngl_renderer_make_current;
  gpu_renderer_class->get_backbuffer = gsk_ngl_renderer_get_backbuffer;
  gpu_renderer_class->wait = gsk_ngl_renderer_wait;
}

static void
gsk_ngl_renderer_init (GskNglRenderer *self)
{
}

/**
 * gsk_ngl_renderer_new:
 *
 * Creates an instance of the new experimental GL renderer.
 *
 * Returns: (transfer full): a new GL renderer
 */
GskRenderer *
gsk_ngl_renderer_new (void)
{
  return g_object_new (GSK_TYPE_NGL_RENDERER, NULL);
}

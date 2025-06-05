#include "config.h"

#include "gskglrenderer.h"

#include "gskgpuimageprivate.h"
#include "gskgpurendererprivate.h"
#include "gskgldeviceprivate.h"
#include "gskglframeprivate.h"
#include "gskglimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdmabufeglprivate.h"
#include "gdk/gdkglcontextprivate.h"

#include <glib/gi18n-lib.h>

/**
 * GskGLRenderer:
 *
 * Renders a GSK rendernode tree with OpenGL.
 *
 * See [class@Gsk.Renderer].
 *
 * Since: 4.2
 */
struct _GskGLRenderer
{
  GskGpuRenderer parent_instance;
};

struct _GskGLRendererClass
{
  GskGpuRendererClass parent_class;
};

G_DEFINE_TYPE (GskGLRenderer, gsk_gl_renderer, GSK_TYPE_GPU_RENDERER)

static GdkDrawContext *
gsk_gl_renderer_create_context (GskGpuRenderer       *renderer,
                                GdkDisplay           *display,
                                GdkSurface           *surface,
                                GskGpuOptimizations  *supported,
                                GError              **error)
{
  GdkGLContext *context;

  if (!gdk_display_prepare_gl (display, error))
    return NULL;

  context = gdk_gl_context_new (display, surface, surface != NULL);

  if (!gdk_gl_context_realize (context, error))
    {
      g_object_unref (context);
      return NULL;
    }

  gdk_gl_context_make_current (context);

  if (!gdk_gl_context_check_version (context, "3.3", "0.0"))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("OpenGL 3.3 required"));
      g_object_unref (context);
      return NULL;
    }

  *supported = -1;

  return GDK_DRAW_CONTEXT (context);
}

static void
gsk_gl_renderer_make_current (GskGpuRenderer *renderer)
{
  gdk_gl_context_make_current (GDK_GL_CONTEXT (gsk_gpu_renderer_get_context (renderer)));
}

static gpointer
gsk_gl_renderer_save_current (GskGpuRenderer *renderer)
{
  GdkGLContext *current;

  current = gdk_gl_context_get_current ();
  if (current)
    g_object_ref (current);

  return current;
}

static void
gsk_gl_renderer_restore_current (GskGpuRenderer *renderer,
                                 gpointer        current)
{
  if (current)
    {
      gdk_gl_context_make_current (current);
      g_object_unref (current);
    }
  else
    gdk_gl_context_clear_current ();
}

static GskGpuImage *
gsk_gl_renderer_get_backbuffer (GskGpuRenderer *renderer)
{
  GdkDrawContext *context;
  GdkSurface *surface;
  guint width, height;

  context = gsk_gpu_renderer_get_context (renderer);
  surface = gdk_draw_context_get_surface (context);
  gdk_draw_context_get_buffer_size (context, &width, &height);

  return gsk_gl_image_new_backbuffer (GSK_GL_DEVICE (gsk_gpu_renderer_get_device (renderer)),
                                      GDK_GL_CONTEXT (context),
                                      GDK_MEMORY_DEFAULT /* FIXME */,
                                      gdk_surface_get_gl_is_srgb (surface),
                                      width,
                                      height);
}

static void
gsk_gl_renderer_unrealize (GskRenderer *renderer)
{
  gdk_gl_context_clear_current ();

  GSK_RENDERER_CLASS (gsk_gl_renderer_parent_class)->unrealize (renderer);
}

static void
gsk_gl_renderer_class_init (GskGLRendererClass *klass)
{
  GskGpuRendererClass *gpu_renderer_class = GSK_GPU_RENDERER_CLASS (klass);
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  gpu_renderer_class->frame_type = GSK_TYPE_GL_FRAME;

  gpu_renderer_class->get_device = gsk_gl_device_get_for_display;
  gpu_renderer_class->create_context = gsk_gl_renderer_create_context;
  gpu_renderer_class->make_current = gsk_gl_renderer_make_current;
  gpu_renderer_class->save_current = gsk_gl_renderer_save_current;
  gpu_renderer_class->restore_current = gsk_gl_renderer_restore_current;
  gpu_renderer_class->get_backbuffer = gsk_gl_renderer_get_backbuffer;

  renderer_class->unrealize = gsk_gl_renderer_unrealize;
}

static void
gsk_gl_renderer_init (GskGLRenderer *self)
{
}

/**
 * gsk_gl_renderer_new:
 *
 * Creates an instance of the GL renderer.
 *
 * Returns: (transfer full): a GL renderer
 */
GskRenderer *
gsk_gl_renderer_new (void)
{
  return g_object_new (GSK_TYPE_GL_RENDERER, NULL);
}

/**
 * GskNglRenderer:
 *
 * A GL based renderer.
 *
 * See [class@Gsk.Renderer].
 */
typedef struct {
  GskRenderer parent_instance;
} GskNglRenderer;

typedef struct {
  GskRendererClass parent_class;
} GskNglRendererClass;

G_DEFINE_TYPE (GskNglRenderer, gsk_ngl_renderer, GSK_TYPE_RENDERER)

static void
gsk_ngl_renderer_init (GskNglRenderer *renderer)
{
}

static gboolean
gsk_ngl_renderer_realize (GskRenderer  *renderer,
                          GdkDisplay   *display,
                          GdkSurface   *surface,
                          gboolean      attach,
                          GError      **error)
{
  g_set_error_literal (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Please use the GL renderer instead");
  return FALSE;
}

static void
gsk_ngl_renderer_class_init (GskNglRendererClass *class)
{
  GSK_RENDERER_CLASS (class)->realize = gsk_ngl_renderer_realize;
}

/**
 * gsk_ngl_renderer_new:
 *
 * Same as gsk_gl_renderer_new().
 *
 * Returns: (transfer full): a GL renderer
 *
 * Deprecated: 4.18: Use gsk_gl_renderer_new()
 */
GskRenderer *
gsk_ngl_renderer_new (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return g_object_new (gsk_ngl_renderer_get_type (), NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
}

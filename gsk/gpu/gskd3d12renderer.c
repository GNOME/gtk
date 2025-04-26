#include "config.h"

#include "gskd3d12renderer.h"

#include "gskgpurendererprivate.h"

#include <glib/gi18n-lib.h>

#ifdef GDK_WINDOWING_WIN32

#include "gskd3d12deviceprivate.h"
#include "gskd3d12frameprivate.h"
#include "gskd3d12imageprivate.h"

#include "gdk/win32/gdkd3d12contextprivate.h"
#include "gdk/win32/gdkprivate-win32.h"
#endif

struct _GskD3d12Renderer
{
  GskGpuRenderer parent_instance;
};

struct _GskD3d12RendererClass
{
  GskGpuRendererClass parent_class;
};

G_DEFINE_TYPE (GskD3d12Renderer, gsk_d3d12_renderer, GSK_TYPE_GPU_RENDERER)

#ifdef GDK_WINDOWING_WIN32
static GdkDrawContext *
gsk_d3d12_renderer_create_context (GskGpuRenderer       *renderer,
                                   GdkDisplay           *display,
                                   GdkSurface           *surface,
                                   GskGpuOptimizations  *supported,
                                   GError              **error)
{
  GdkD3d12Context *context;

  if (!GDK_IS_WIN32_DISPLAY (display))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "%s does not support Direct3D 12", G_OBJECT_TYPE_NAME (display));
      return NULL;
    }

  context = gdk_d3d12_context_new (GDK_WIN32_DISPLAY (display), surface, error);
  if (context == NULL)
    return NULL;

  *supported = -1;

  return GDK_DRAW_CONTEXT (context);
}

static void
gsk_d3d12_renderer_make_current (GskGpuRenderer *renderer)
{
}

static gpointer
gsk_d3d12_renderer_save_current (GskGpuRenderer *renderer)
{
  return NULL;
}

static void
gsk_d3d12_renderer_restore_current (GskGpuRenderer *renderer,
                                    gpointer        current)
{
}

static GskGpuImage *
gsk_d3d12_renderer_get_backbuffer (GskGpuRenderer *renderer)
{
  GdkD3d12Context *context;
  IDXGISwapChain3 *swap_chain;
  ID3D12Resource *resource;
  GskGpuImage *result;

  context = GDK_D3D12_CONTEXT (gsk_gpu_renderer_get_context (renderer));
  swap_chain = gdk_d3d12_context_get_swap_chain (context);

  hr_warn (IDXGISwapChain3_GetBuffer (swap_chain,
                                      IDXGISwapChain3_GetCurrentBackBufferIndex (swap_chain),
                                      &IID_ID3D12Resource,
                                      (void **) &resource));

  result = gsk_d3d12_image_new_for_resource (resource, TRUE);

  gdk_win32_com_clear (&resource);

  return result;
}

#else /* !GDK_WINDOWING_WIN32 */

static gboolean
gsk_d3d12_renderer_realize (GskRenderer  *renderer,
                            GdkDisplay   *display,
                            GdkSurface   *surface,
                            gboolean      attach,
                            GError      **error)
{
  g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                       _("Direct3D 12 is only available on Windows"));

  return FALSE;
}
#endif /* GDK_WINDOWING_WIN32 */

static void
gsk_d3d12_renderer_class_init (GskD3d12RendererClass *klass)
{
#ifdef GDK_WINDOWING_WIN32
  GskGpuRendererClass *gpu_renderer_class = GSK_GPU_RENDERER_CLASS (klass);

  gpu_renderer_class->frame_type = GSK_TYPE_D3D12_FRAME;

  gpu_renderer_class->get_device = gsk_d3d12_device_get_for_display;
  gpu_renderer_class->create_context = gsk_d3d12_renderer_create_context;
  gpu_renderer_class->make_current = gsk_d3d12_renderer_make_current;
  gpu_renderer_class->save_current = gsk_d3d12_renderer_save_current;
  gpu_renderer_class->restore_current = gsk_d3d12_renderer_restore_current;
  gpu_renderer_class->get_backbuffer = gsk_d3d12_renderer_get_backbuffer;
#else
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_d3d12_renderer_realize;
#endif
}

static void
gsk_d3d12_renderer_init (GskD3d12Renderer *self)
{
}

/**
 * gsk_d3d12_renderer_new:
 *
 * Creates a new Direct3D12 renderer.
 *
 * The D3D12 renderer is a renderer that uses the Windows Direct3D 12
 * for rendering.
 *
 * This renderer will fail to realize when GTK was not compiled on
 * Windows.
 *
 * Returns: a new D3d12 renderer
 *
 * Since: 4.20
 **/
GskRenderer *
gsk_d3d12_renderer_new (void)
{
  return g_object_new (GSK_TYPE_D3D12_RENDERER, NULL);
}

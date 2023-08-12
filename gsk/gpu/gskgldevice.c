#include "config.h"

#include "gskgldeviceprivate.h"
#include "gskglimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkglcontextprivate.h"

#include <glib/gi18n-lib.h>

struct _GskGLDevice
{
  GskGpuDevice parent_instance;
};

struct _GskGLDeviceClass
{
  GskGpuDeviceClass parent_class;
};

G_DEFINE_TYPE (GskGLDevice, gsk_gl_device, GSK_TYPE_GPU_DEVICE)

static GskGpuImage *
gsk_gl_device_create_offscreen_image (GskGpuDevice   *device,
                                      GdkMemoryDepth  depth,
                                      gsize           width,
                                      gsize           height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           gdk_memory_depth_get_format (depth),
                           width,
                           height);
}

static GskGpuImage *
gsk_gl_device_create_upload_image (GskGpuDevice    *device,
                                   GdkMemoryFormat  format,
                                   gsize            width,
                                   gsize            height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           format,
                           width,
                           height);
}

static void
gsk_gl_device_finalize (GObject *object)
{
  GskGLDevice *self = GSK_GL_DEVICE (object);
  GskGpuDevice *device = GSK_GPU_DEVICE (self);

  g_object_steal_data (G_OBJECT (gsk_gpu_device_get_display (device)), "-gsk-gl-device");

  G_OBJECT_CLASS (gsk_gl_device_parent_class)->finalize (object);
}

static void
gsk_gl_device_class_init (GskGLDeviceClass *klass)
{
  GskGpuDeviceClass *gpu_device_class = GSK_GPU_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_device_class->create_offscreen_image = gsk_gl_device_create_offscreen_image;
  gpu_device_class->create_upload_image = gsk_gl_device_create_upload_image;

  object_class->finalize = gsk_gl_device_finalize;
}

static void
gsk_gl_device_init (GskGLDevice *self)
{
}

GskGpuDevice *
gsk_gl_device_get_for_display (GdkDisplay  *display,
                               GError     **error)
{
  GskGLDevice *self;
  GdkGLContext *context;

  self = g_object_get_data (G_OBJECT (display), "-gsk-gl-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_display_prepare_gl (display, error))
    return NULL;

  context = gdk_display_get_gl_context (display);

  /* GLES 2 is not supported */
  if (!gdk_gl_context_check_version (context, "3.0", "3.0"))
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                   _("OpenGL ES 2.0 is not supported by this renderer."));
      return NULL;
    }

  self = g_object_new (GSK_TYPE_GL_DEVICE, NULL);

  gsk_gpu_device_setup (GSK_GPU_DEVICE (self), display);

  g_object_set_data (G_OBJECT (display), "-gsk-gl-device", self);

  return GSK_GPU_DEVICE (self);
}

void
gsk_gl_device_find_gl_format (GskGLDevice     *self,
                              GdkMemoryFormat  format,
                              GdkMemoryFormat *out_format,
                              GLint           *out_gl_internal_format,
                              GLenum          *out_gl_format,
                              GLenum          *out_gl_type,
                              GLint            out_swizzle[4])
{
  gdk_memory_format_gl_format (format,
                               out_gl_internal_format,
                               out_gl_format,
                               out_gl_type,
                               out_swizzle);
}


#include "config.h"

#include "gskgldeviceprivate.h"

#include "gskdebugprivate.h"
#include "gskgpushaderflagsprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskglbufferprivate.h"
#include "gskglimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkglcontextprivate.h"
#include "gdk/gdkprofilerprivate.h"

#include <glib/gi18n-lib.h>

struct _GskGLDevice
{
  GskGpuDevice parent_instance;

  const char *version_string;
  GdkGLAPI api;

  guint sampler_ids[GSK_GPU_SAMPLER_N_SAMPLERS];
};

struct _GskGLDeviceClass
{
  GskGpuDeviceClass parent_class;
};

G_DEFINE_TYPE (GskGLDevice, gsk_gl_device, GSK_TYPE_GPU_DEVICE)

static GskGpuImage *
gsk_gl_device_create_offscreen_image (GskGpuDevice   *device,
                                      gboolean        with_mipmap,
                                      GdkMemoryFormat format,
                                      gboolean        is_srgb,
                                      gsize           width,
                                      gsize           height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           format,
                           is_srgb,
                           GSK_GPU_IMAGE_RENDERABLE | GSK_GPU_IMAGE_FILTERABLE,
                           width,
                           height);
}

static GskGpuImage *
gsk_gl_device_create_upload_image (GskGpuDevice     *device,
                                   gboolean          with_mipmap,
                                   GdkMemoryFormat   format,
                                   GskGpuConversion  conv,
                                   gsize             width,
                                   gsize             height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           format,
                           conv == GSK_GPU_CONVERSION_SRGB,
                           0,
                           width,
                           height);
}

static GskGpuImage *
gsk_gl_device_create_download_image (GskGpuDevice   *device,
                                     GdkMemoryDepth  depth,
                                     gsize           width,
                                     gsize           height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           gdk_memory_depth_get_format (depth),
                           FALSE,
                           GSK_GPU_IMAGE_RENDERABLE | GSK_GPU_IMAGE_DOWNLOADABLE,
                           width,
                           height);
}

static GskGpuImage *
gsk_gl_device_create_atlas_image (GskGpuDevice *device,
                                  gsize         width,
                                  gsize         height)
{
  GskGLDevice *self = GSK_GL_DEVICE (device);

  return gsk_gl_image_new (self,
                           GDK_MEMORY_DEFAULT,
                           FALSE,
                           GSK_GPU_IMAGE_RENDERABLE,
                           width,
                           height);
}

static void
gsk_gl_device_make_current (GskGpuDevice *device)
{
  gdk_gl_context_make_current (gdk_display_get_gl_context (gsk_gpu_device_get_display (device)));
}

static void
gsk_gl_device_finalize (GObject *object)
{
  GskGLDevice *self = GSK_GL_DEVICE (object);
  GskGpuDevice *device = GSK_GPU_DEVICE (self);

  g_object_steal_data (G_OBJECT (gsk_gpu_device_get_display (device)), "-gsk-gl-device");

  gdk_gl_context_make_current (gdk_display_get_gl_context (gsk_gpu_device_get_display (device)));

  glDeleteSamplers (G_N_ELEMENTS (self->sampler_ids), self->sampler_ids);

  G_OBJECT_CLASS (gsk_gl_device_parent_class)->finalize (object);
}

static void
gsk_gl_device_class_init (GskGLDeviceClass *klass)
{
  GskGpuDeviceClass *gpu_device_class = GSK_GPU_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_device_class->create_offscreen_image = gsk_gl_device_create_offscreen_image;
  gpu_device_class->create_atlas_image = gsk_gl_device_create_atlas_image;
  gpu_device_class->create_upload_image = gsk_gl_device_create_upload_image;
  gpu_device_class->create_download_image = gsk_gl_device_create_download_image;
  gpu_device_class->make_current = gsk_gl_device_make_current;

  object_class->finalize = gsk_gl_device_finalize;
}

static void
gsk_gl_device_init (GskGLDevice *self)
{
}

static void
gsk_gl_device_setup_samplers (GskGLDevice *self)
{
  struct {
    GLuint min_filter;
    GLuint mag_filter;
    GLuint wrap;
  } sampler_flags[GSK_GPU_SAMPLER_N_SAMPLERS] = {
    [GSK_GPU_SAMPLER_DEFAULT] = {
      .min_filter = GL_LINEAR,
      .mag_filter = GL_LINEAR,
      .wrap = GL_CLAMP_TO_EDGE,
    },
    [GSK_GPU_SAMPLER_TRANSPARENT] = {
      .min_filter = GL_LINEAR,
      .mag_filter = GL_LINEAR,
      .wrap = GL_CLAMP_TO_BORDER,
    },
    [GSK_GPU_SAMPLER_REPEAT] = {
      .min_filter = GL_LINEAR,
      .mag_filter = GL_LINEAR,
      .wrap = GL_REPEAT,
    },
    [GSK_GPU_SAMPLER_REFLECT] = {
      .min_filter = GL_LINEAR,
      .mag_filter = GL_LINEAR,
      .wrap = GL_MIRRORED_REPEAT,
    },
    [GSK_GPU_SAMPLER_NEAREST] = {
      .min_filter = GL_NEAREST,
      .mag_filter = GL_NEAREST,
      .wrap = GL_CLAMP_TO_EDGE,
    },
    [GSK_GPU_SAMPLER_MIPMAP_DEFAULT] = {
      .min_filter = GL_LINEAR_MIPMAP_LINEAR,
      .mag_filter = GL_LINEAR,
      .wrap = GL_CLAMP_TO_EDGE,
    }
  };
  guint i;

  glGenSamplers (G_N_ELEMENTS (self->sampler_ids), self->sampler_ids);

  for (i = 0; i < G_N_ELEMENTS (self->sampler_ids); i++)
    {
      glSamplerParameteri (self->sampler_ids[i], GL_TEXTURE_MIN_FILTER, sampler_flags[i].min_filter);
      glSamplerParameteri (self->sampler_ids[i], GL_TEXTURE_MAG_FILTER, sampler_flags[i].mag_filter);
      glSamplerParameteri (self->sampler_ids[i], GL_TEXTURE_WRAP_S, sampler_flags[i].wrap);
      glSamplerParameteri (self->sampler_ids[i], GL_TEXTURE_WRAP_T, sampler_flags[i].wrap);
    }
}

GskGpuDevice *
gsk_gl_device_get_for_display (GdkDisplay  *display,
                               GError     **error)
{
  GskGLDevice *self;
  GdkGLContext *context;
  GLint max_texture_size, globals_alignment;

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
                   _("OpenGL ES 3.0 is not supported by this renderer."));
      return NULL;
    }

  self = g_object_new (GSK_TYPE_GL_DEVICE, NULL);

  gdk_gl_context_make_current (context);

  glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_texture_size);
  glGetIntegerv (GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &globals_alignment);

  gsk_gpu_device_setup (GSK_GPU_DEVICE (self),
                        display,
                        max_texture_size,
                        GSK_GPU_DEVICE_DEFAULT_TILE_SIZE,
                        globals_alignment);

  self->version_string = gdk_gl_context_get_glsl_version_string (context);
  self->api = gdk_gl_context_get_api (context);
  gsk_gl_device_setup_samplers (self);

  g_object_set_data (G_OBJECT (display), "-gsk-gl-device", self);

  return GSK_GPU_DEVICE (self);
}

gboolean
gsk_gl_device_has_gl_feature (GskGLDevice   *self,
                              GdkGLFeatures  feature)
{
  GdkDisplay *display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));
  GdkGLContext *context = gdk_display_get_gl_context (display);

  return gdk_gl_context_has_feature (context, feature);
}

const char *
gsk_gl_device_get_version_string (GskGLDevice *self)
{
  return self->version_string;
}

GdkGLAPI
gsk_gl_device_get_gl_api (GskGLDevice *self)
{
  return self->api;
}

GLuint
gsk_gl_device_get_sampler_id (GskGLDevice   *self,
                              GskGpuSampler  sampler)
{
  g_return_val_if_fail (sampler < G_N_ELEMENTS (self->sampler_ids), 0);

  return self->sampler_ids[sampler];
}

static gboolean
gsk_gl_device_get_format_flags (GskGLDevice      *self,
                                GdkGLContext     *context,
                                GdkMemoryFormat   format,
                                GdkSwizzle        swizzle,
                                GskGpuImageFlags *out_flags)
{
  GdkGLMemoryFlags gl_flags;

  *out_flags = 0;

  gl_flags = gdk_gl_context_get_format_flags (context, format);

  if (!(gl_flags & GDK_GL_FORMAT_USABLE))
    return FALSE;

  if (gdk_shader_op_get_n_shaders (gdk_memory_format_get_default_shader_op (format)) <= 1)
    *out_flags |= GSK_GPU_IMAGE_DOWNLOADABLE;

  if ((gl_flags & GDK_GL_FORMAT_RENDERABLE) && gdk_swizzle_is_framebuffer_compatible (swizzle))
    *out_flags |= GSK_GPU_IMAGE_RENDERABLE;
  if (gl_flags & GDK_GL_FORMAT_FILTERABLE)
    *out_flags |= GSK_GPU_IMAGE_FILTERABLE;
  if ((gl_flags & (GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE)) == (GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE))
    {
      *out_flags |= GSK_GPU_IMAGE_CAN_MIPMAP;
      if (gdk_swizzle_is_identity (swizzle))
        *out_flags |= GSK_GPU_IMAGE_BLIT;
    }

  return TRUE;
}

void
gsk_gl_device_find_gl_format (GskGLDevice      *self,
                              GdkMemoryFormat   format,
                              GskGpuImageFlags  required_flags,
                              GdkMemoryFormat  *out_format,
                              GskGpuImageFlags *out_flags,
                              GLint            *out_gl_internal_format,
                              GLint            *out_gl_internal_srgb_format,
                              GLenum           *out_gl_format,
                              GLenum           *out_gl_type,
                              GdkSwizzle       *out_swizzle)
{
  GdkGLContext *context = gdk_gl_context_get_current ();
  GskGpuImageFlags flags;
  GdkMemoryFormat alt_format;
  const GdkMemoryFormat *fallbacks;
  gsize i;
  GdkSwizzle swizzle;

  /* First, try the actual format */
  if (gdk_memory_format_gl_format (format,
                                   0,
                                   gdk_gl_context_get_use_es (context),
                                   out_gl_internal_format,
                                   out_gl_internal_srgb_format,
                                   out_gl_format,
                                   out_gl_type,
                                   out_swizzle) &&
      gsk_gl_device_get_format_flags (self, context, format, *out_swizzle, &flags) &&
      ((flags & required_flags) == required_flags))
    {
      *out_format = format;
      *out_flags = flags;
      return;
    }

  /* Second, try the potential RGBA format */
  if (gdk_memory_format_get_rgba_format (format,
                                         &alt_format,
                                         &swizzle) &&
      gdk_memory_format_gl_format (alt_format,
                                   0,
                                   gdk_gl_context_get_use_es (context),
                                   out_gl_internal_format,
                                   out_gl_internal_srgb_format,
                                   out_gl_format,
                                   out_gl_type,
                                   out_swizzle))
    {
      *out_swizzle = swizzle;
      if (gsk_gl_device_get_format_flags (self, context, alt_format, *out_swizzle, &flags) &&
          ((flags & required_flags) == required_flags))
        {
          *out_format = format;
          *out_flags = flags;
          return;
        }
    }

  /* Next, try the fallbacks */
  fallbacks = gdk_memory_format_get_fallbacks (format);
  for (i = 0; fallbacks[i] != -1; i++)
    {
      if (gdk_memory_format_gl_format (fallbacks[i],
                                       0,
                                       gdk_gl_context_get_use_es (context),
                                       out_gl_internal_format,
                                       out_gl_internal_srgb_format,
                                       out_gl_format,
                                       out_gl_type,
                                       out_swizzle) &&
          gsk_gl_device_get_format_flags (self, context, fallbacks[i], *out_swizzle, &flags) &&
          ((flags & required_flags) == required_flags))
        {
          *out_format = fallbacks[i];
          *out_flags = flags;
          return;
        }
    }

  /* fallbacks will always fallback to a supported format */
  g_assert_not_reached ();
}


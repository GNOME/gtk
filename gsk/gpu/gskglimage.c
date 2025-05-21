#include "config.h"

#include "gskglimageprivate.h"

#include "gskgpuutilsprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkglcontextprivate.h"

struct _GskGLImage
{
  GskGpuImage parent_instance;

  guint texture_id[3];
  guint memory_id[3];
  guint semaphore_id;
  guint framebuffer_id;

  GLint gl_internal_format[3];
  GLenum gl_format[3];
  GLenum gl_type[3];

  guint owns_texture : 1;
};

struct _GskGLImageClass
{
  GskGpuImageClass parent_class;
};

G_DEFINE_TYPE (GskGLImage, gsk_gl_image, GSK_TYPE_GPU_IMAGE)

static void
gsk_gl_image_get_projection_matrix (GskGpuImage       *image,
                                    graphene_matrix_t *out_projection)
{
  GskGLImage *self = GSK_GL_IMAGE (image);

  GSK_GPU_IMAGE_CLASS (gsk_gl_image_parent_class)->get_projection_matrix (image, out_projection);

  if (self->texture_id[0] == 0)
    graphene_matrix_scale (out_projection, 1.f, -1.f, 1.f);
}

static gsize
gsk_gl_image_get_n_textures (GskGLImage *self)
{
  gsize n;

  for (n = 1; n < 3; n++)
    {
      if (self->texture_id[n] == 0)
        break;
    }

  return n;
}

static void
gsk_gl_image_finalize (GObject *object)
{
  GskGLImage *self = GSK_GL_IMAGE (object);
  gsize n_textures = gsk_gl_image_get_n_textures (self);


  if (self->texture_id[0] && self->framebuffer_id)
    glDeleteFramebuffers (1, &self->framebuffer_id);

  if (self->owns_texture)
    glDeleteTextures (n_textures, self->texture_id);

  if (self->memory_id[0])
    glDeleteMemoryObjectsEXT (n_textures, self->memory_id);

  if (self->semaphore_id)
    glDeleteSemaphoresEXT (1, &self->semaphore_id);

  G_OBJECT_CLASS (gsk_gl_image_parent_class)->finalize (object);
}

static void
gsk_gl_image_class_init (GskGLImageClass *klass)
{
  GskGpuImageClass *image_class = GSK_GPU_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_projection_matrix = gsk_gl_image_get_projection_matrix;

  object_class->finalize = gsk_gl_image_finalize;
}

static void
gsk_gl_image_init (GskGLImage *self)
{
}

GskGpuImage *
gsk_gl_image_new_backbuffer (GskGLDevice    *device,
                             GdkGLContext   *context,
                             GdkMemoryFormat format,
                             gboolean        is_srgb,
                             gsize           width,
                             gsize           height)
{
  GskGLImage *self;
  GskGpuImageFlags flags;
  GskGpuConversion conv;
  GdkSwizzle swizzle;
  GLint gl_internal_format, gl_internal_srgb_format;

  self = g_object_new (GSK_TYPE_GL_IMAGE, NULL);

  /* We only do this so these variables get initialized */
  gsk_gl_device_find_gl_format (device,
                                format,
                                0,
                                &format,
                                &flags,
                                &gl_internal_format,
                                &gl_internal_srgb_format,
                                &self->gl_format[0],
                                &self->gl_type[0],
                                &swizzle);

  if (is_srgb)
    {
      if (gl_internal_srgb_format != -1)
        self->gl_internal_format[0] = gl_internal_srgb_format;
      else /* FIXME: Happens when the driver uses formats that it does not expose */
        self->gl_internal_format[0] = gl_internal_format;
      conv = GSK_GPU_CONVERSION_SRGB;
    }
  else
    {
      self->gl_internal_format[0] = gl_internal_format;
      conv = GSK_GPU_CONVERSION_NONE;
    }

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self),
                       flags,
                       conv,
                       gdk_memory_format_get_default_shader_op (format),
                       format, width, height);

  /* texture_id == 0 means backbuffer */

  /* Check for non-standard framebuffer binding as we might not be using
   * the default framebuffer on systems like macOS where we've bound an
   * IOSurface to a GL_TEXTURE_RECTANGLE. Otherwise, no scissor clip will
   * be applied in the command queue causing overdrawing.
   */
  self->framebuffer_id = GDK_GL_CONTEXT_GET_CLASS (context)->get_default_framebuffer (context);

  return GSK_GPU_IMAGE (self);
}

GskGpuImage *
gsk_gl_image_new (GskGLDevice      *device,
                  GdkMemoryFormat   format,
                  gboolean          try_srgb,
                  GskGpuImageFlags  required_flags,
                  gsize             width,
                  gsize             height)
{
  GskGLImage *self;
  GdkSwizzle swizzle;
  GskGpuImageFlags flags;
  GskGpuConversion conv;
  GLint gl_internal_format, gl_internal_srgb_format;
  gsize max_size;

  max_size = gsk_gpu_device_get_max_image_size (GSK_GPU_DEVICE (device));
  if (width > max_size || height > max_size)
    return NULL;

  self = g_object_new (GSK_TYPE_GL_IMAGE, NULL);

  gsk_gl_device_find_gl_format (device,
                                format,
                                required_flags,
                                &format,
                                &flags,
                                &gl_internal_format,
                                &gl_internal_srgb_format,
                                &self->gl_format[0],
                                &self->gl_type[0],
                                &swizzle);

  if (try_srgb && gl_internal_srgb_format != -1)
    {
      self->gl_internal_format[0] = gl_internal_srgb_format;
      conv = GSK_GPU_CONVERSION_SRGB;
    }
  else
    {
      self->gl_internal_format[0] = gl_internal_format;
      conv = GSK_GPU_CONVERSION_NONE;
    }

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self),
                       flags,
                       conv,
                       gdk_memory_format_get_default_shader_op (format),
                       format,
                       width, height);

  glGenTextures (1, self->texture_id);
  self->owns_texture = TRUE;

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, self->texture_id[0]);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D (GL_TEXTURE_2D, 0, self->gl_internal_format[0], width, height, 0, self->gl_format[0], self->gl_type[0], NULL);

  /* Only apply swizzle if really needed, might not even be
   * supported if default values are set
   */
  if (!gdk_swizzle_is_identity (swizzle))
    {
      GLint gl_swizzle[4];

      gdk_swizzle_to_gl (swizzle, gl_swizzle);
      /* Set each channel independently since GLES 3.0 doesn't support the iv method */
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, gl_swizzle[0]);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, gl_swizzle[1]);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, gl_swizzle[2]);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, gl_swizzle[3]);
    }

  return GSK_GPU_IMAGE (self);
}

GskGpuImage *
gsk_gl_image_new_for_texture (GskGLDevice      *device,
                              GdkTexture       *owner,
                              GLuint            tex_id,
                              GLuint            mem_id,
                              GLuint            semaphore_id,
                              gboolean          take_ownership,
                              GskGpuImageFlags  extra_flags,
                              GskGpuConversion  conv)
{
  GdkMemoryFormat format, real_format;
  GskGpuImageFlags flags;
  GskGLImage *self;
  GLint gl_internal_format, gl_internal_srgb_format;
  GdkSwizzle swizzle;

  format = gdk_texture_get_format (owner);

  self = g_object_new (GSK_TYPE_GL_IMAGE, NULL);

  gsk_gl_device_find_gl_format (device,
                                format,
                                0,
                                &real_format,
                                &flags,
                                &gl_internal_format,
                                &gl_internal_srgb_format,
                                &self->gl_format[0],
                                &self->gl_type[0],
                                &swizzle);
  
  self->gl_internal_format[0] = gl_internal_format;

  if (format != real_format)
    {
      flags = 0;
    }
  else
    {
      flags &= ~(GSK_GPU_IMAGE_CAN_MIPMAP | GSK_GPU_IMAGE_MIPMAP);
      if (extra_flags & GSK_GPU_IMAGE_EXTERNAL)
        flags &= ~(GSK_GPU_IMAGE_BLIT | GSK_GPU_IMAGE_DOWNLOADABLE);
    }
  
  gsk_gpu_image_setup (GSK_GPU_IMAGE (self),
                       flags | extra_flags,
                       conv,
                       gdk_memory_format_get_default_shader_op (format),
                       format,
                       gdk_texture_get_width (owner),
                       gdk_texture_get_height (owner));
  gsk_gpu_image_toggle_ref_texture (GSK_GPU_IMAGE (self), owner);

  self->texture_id[0] = tex_id;
  self->memory_id[0] = mem_id;
  self->semaphore_id = semaphore_id;
  self->owns_texture = take_ownership;

  /* XXX: We're waiting for the semaphore here, which is quite early.
   * And unexpected.
   * But it means we only wait once.
   * So we got that going for us, which is nice. */
  if (semaphore_id)
    {
      glWaitSemaphoreEXT (semaphore_id,
                          0, NULL,
                          1, &tex_id, (GLenum[1]) {GL_LAYOUT_GENERAL_EXT });
    }
  return GSK_GPU_IMAGE (self);
}

void
gsk_gl_image_bind_textures (GskGLImage *self,
                            GLenum      target)
{
  if (gsk_gpu_image_get_flags (GSK_GPU_IMAGE (self)) & GSK_GPU_IMAGE_EXTERNAL)
    {
      glActiveTexture (target);
      glBindTexture (GL_TEXTURE_EXTERNAL_OES, self->texture_id[0]);
    }
  else
    {
      gsize i;

      for (i = 0; self->texture_id[i]; i++)
        {
          glActiveTexture (target + i);
          glBindTexture (GL_TEXTURE_2D, self->texture_id[i]);
        }
    }
}

void
gsk_gl_image_bind_framebuffer_target (GskGLImage *self,
                                      GLenum      target)
{
  GLenum status;

  if (self->framebuffer_id)
    {
      glBindFramebuffer (target, self->framebuffer_id);
      return;
    }

  /* We're the renderbuffer */
  if (self->texture_id[0] == 0)
    {
      glBindFramebuffer (target, 0);
      return;
    }

  g_assert (self->texture_id[1] == 0);

  glGenFramebuffers (1, &self->framebuffer_id);
  glBindFramebuffer (target, self->framebuffer_id);
  glFramebufferTexture2D (target, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self->texture_id[0], 0);
  status = glCheckFramebufferStatus (target);

  switch (status)
  {
    case GL_FRAMEBUFFER_COMPLETE:
      break;

    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      g_critical ("glCheckFramebufferStatus() returned GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT. Expect broken rendering.");
      break;

    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
      g_critical ("glCheckFramebufferStatus() returned GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS. Expect broken rendering.");
      break;

    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      g_critical ("glCheckFramebufferStatus() returned GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT. Expect broken rendering.");
      break;

    case GL_FRAMEBUFFER_UNSUPPORTED:
      g_critical ("glCheckFramebufferStatus() returned GL_FRAMEBUFFER_UNSUPPORTED. Expect broken rendering.");
      break;

    default:
      g_critical ("glCheckFramebufferStatus() returned %u (0x%x). Expect broken rendering.", status, status);
      break;
    }
}

void
gsk_gl_image_bind_framebuffer (GskGLImage *self)
{
  gsk_gl_image_bind_framebuffer_target (self, GL_FRAMEBUFFER);
}

gboolean
gsk_gl_image_is_flipped (GskGLImage *self)
{
  return self->texture_id[0] == 0;
}

GLint
gsk_gl_image_get_gl_internal_format (GskGLImage *self,
                                     gsize       nth)
{
  return self->gl_internal_format[nth];
}

GLenum
gsk_gl_image_get_gl_format (GskGLImage *self,
                            gsize       nth)
{
  return self->gl_format[nth];
}

GLenum
gsk_gl_image_get_gl_type (GskGLImage *self,
                          gsize       nth)
{
  return self->gl_type[nth];
}

GLuint
gsk_gl_image_get_texture_id (GskGLImage *self,
                             gsize       nth)
{
  return self->texture_id[nth];
}

void
gsk_gl_image_steal_texture_ownership (GskGLImage *self)
{
  g_assert (self->texture_id[0]);
  g_assert (self->owns_texture);

  self->owns_texture = FALSE;
}


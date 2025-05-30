#include "config.h"

#include "gskglimageprivate.h"

#include "gskgpuutilsprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkglcontextprivate.h"

struct _GskGLImage
{
  GskGpuImage parent_instance;

  guint texture_id;
  guint memory_id;
  guint semaphore_id;
  guint framebuffer_id;

  GLint gl_internal_format;
  GLenum gl_format;
  GLenum gl_type;

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

  if (self->texture_id == 0)
    graphene_matrix_scale (out_projection, 1.f, -1.f, 1.f);
}

static void
gsk_gl_image_finalize (GObject *object)
{
  GskGLImage *self = GSK_GL_IMAGE (object);

  if (self->texture_id && self->framebuffer_id)
    glDeleteFramebuffers (1, &self->framebuffer_id);

  if (self->owns_texture)
    glDeleteTextures (1, &self->texture_id);

  if (self->memory_id)
    glDeleteMemoryObjectsEXT (1, &self->memory_id);

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

static gboolean
swizzle_is_identity (const GLint swizzle[4])
{
  if (swizzle[0] != GL_RED ||
      swizzle[1] != GL_GREEN ||
      swizzle[2] != GL_BLUE ||
      swizzle[3] != GL_ALPHA)
    return FALSE;

  return TRUE;
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
  GLint swizzle[4];
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
                                &self->gl_format,
                                &self->gl_type,
                                swizzle);

  if (is_srgb)
    {
      if (gl_internal_srgb_format != -1)
        self->gl_internal_format = gl_internal_srgb_format;
      else /* FIXME: Happens when the driver uses formats that it does not expose */
        self->gl_internal_format = gl_internal_format;
      conv = GSK_GPU_CONVERSION_SRGB;
    }
  else
    {
      self->gl_internal_format = gl_internal_format;
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
  GLint swizzle[4];
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
                                &self->gl_format,
                                &self->gl_type,
                                swizzle);

  if (try_srgb && gl_internal_srgb_format != -1)
    {
      self->gl_internal_format = gl_internal_srgb_format;
      conv = GSK_GPU_CONVERSION_SRGB;
    }
  else
    {
      self->gl_internal_format = gl_internal_format;
      conv = GSK_GPU_CONVERSION_NONE;
    }

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self),
                       flags,
                       conv,
                       gdk_memory_format_get_default_shader_op (format),
                       format,
                       width, height);

  glGenTextures (1, &self->texture_id);
  self->owns_texture = TRUE;

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, self->texture_id);
 
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D (GL_TEXTURE_2D, 0, self->gl_internal_format, width, height, 0, self->gl_format, self->gl_type, NULL);

  /* Only apply swizzle if really needed, might not even be
   * supported if default values are set
   */
  if (!swizzle_is_identity (swizzle))
    {
      /* Set each channel independently since GLES 3.0 doesn't support the iv method */
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, swizzle[0]);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, swizzle[1]);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, swizzle[2]);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, swizzle[3]);
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
  GLint swizzle[4];

  format = gdk_texture_get_format (owner);

  self = g_object_new (GSK_TYPE_GL_IMAGE, NULL);

  gsk_gl_device_find_gl_format (device,
                                format,
                                0,
                                &real_format,
                                &flags,
                                &gl_internal_format,
                                &gl_internal_srgb_format,
                                &self->gl_format,
                                &self->gl_type,
                                swizzle);
  
  self->gl_internal_format = gl_internal_format;

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

  self->texture_id = tex_id;
  self->memory_id = mem_id;
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
gsk_gl_image_bind_texture (GskGLImage *self)
{
  if (gsk_gpu_image_get_flags (GSK_GPU_IMAGE (self)) & GSK_GPU_IMAGE_EXTERNAL)
    glBindTexture (GL_TEXTURE_EXTERNAL_OES, self->texture_id);
  else
    glBindTexture (GL_TEXTURE_2D, self->texture_id);
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
  if (self->texture_id == 0)
    {
      glBindFramebuffer (target, 0);
      return;
    }

  glGenFramebuffers (1, &self->framebuffer_id);
  glBindFramebuffer (target, self->framebuffer_id);
  glFramebufferTexture2D (target, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self->texture_id, 0);
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
  return self->texture_id == 0;
}

GLint
gsk_gl_image_get_gl_internal_format (GskGLImage *self)
{
  return self->gl_internal_format;
}

GLenum
gsk_gl_image_get_gl_format (GskGLImage *self)
{
  return self->gl_format;
}

GLenum
gsk_gl_image_get_gl_type (GskGLImage *self)
{
  return self->gl_type;
}

GLuint
gsk_gl_image_get_texture_id (GskGLImage *self)
{
  return self->texture_id;
}

void
gsk_gl_image_steal_texture_ownership (GskGLImage *self)
{
  g_assert (self->texture_id);
  g_assert (self->owns_texture);

  self->owns_texture = FALSE;
}


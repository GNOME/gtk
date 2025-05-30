#include "config.h"

#include "gskglframeprivate.h"

#include "gskgpuglobalsopprivate.h"
#include "gskgpuopprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskgpuutilsprivate.h"
#include "gskglbufferprivate.h"
#include "gskgldeviceprivate.h"
#include "gskglimageprivate.h"

#include "gdkdmabuftextureprivate.h"
#include "gdkdmabufeglprivate.h"
#include "gdkglcontextprivate.h"
#include "gdkgltextureprivate.h"

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkd3d12textureprivate.h"
#endif
struct _GskGLFrame
{
  GskGpuFrame parent_instance;

  guint next_texture_slot;
  GLsync sync;

  GHashTable *vaos;
};

struct _GskGLFrameClass
{
  GskGpuFrameClass parent_class;
};

G_DEFINE_TYPE (GskGLFrame, gsk_gl_frame, GSK_TYPE_GPU_FRAME)

static gboolean
gsk_gl_frame_is_busy (GskGpuFrame *frame)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  if (!self->sync)
    return FALSE;

  return glClientWaitSync (self->sync, 0, 0) == GL_TIMEOUT_EXPIRED;
}

static void
gsk_gl_frame_wait (GskGpuFrame *frame)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  if (!self->sync)
    return;

  glClientWaitSync (self->sync, GL_SYNC_FLUSH_COMMANDS_BIT, G_MAXUINT64);
}

static void
gsk_gl_frame_cleanup (GskGpuFrame *frame)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  if (self->sync)
    {
      glClientWaitSync (self->sync, GL_SYNC_FLUSH_COMMANDS_BIT, G_MAXUINT64);

      /* can't use g_clear_pointer() on glDeleteSync(), see MR !7294 */
      glDeleteSync (self->sync);
      self->sync = NULL;
    }

  self->next_texture_slot = 0;

  GSK_GPU_FRAME_CLASS (gsk_gl_frame_parent_class)->cleanup (frame);
}

static GskGpuImage *
gsk_gl_frame_upload_texture (GskGpuFrame  *frame,
                             gboolean      with_mipmap,
                             GdkTexture   *texture)
{
  if (GDK_IS_GL_TEXTURE (texture))
    {
      GdkGLTexture *gl_texture = GDK_GL_TEXTURE (texture);

      if (gdk_gl_context_is_shared (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                    gdk_gl_texture_get_context (gl_texture)))
        {
          GskGpuImage *image;
          GLsync sync;

          image = gsk_gl_image_new_for_texture (GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                texture,
                                                1,
                                                (GLuint[1]) { gdk_gl_texture_get_id (gl_texture), },
                                                NULL,
                                                0,
                                                FALSE,
                                                gdk_gl_texture_has_mipmap (gl_texture) ? (GSK_GPU_IMAGE_CAN_MIPMAP | GSK_GPU_IMAGE_MIPMAP) : 0,
                                                GSK_GPU_CONVERSION_NONE);

          /* This is a hack, but it works */
          sync = gdk_gl_texture_get_sync (gl_texture);
          if (sync)
            glWaitSync (sync, 0, GL_TIMEOUT_IGNORED);

          return image;
        }
    }
#if defined(HAVE_DMABUF) && defined (HAVE_EGL)
  else if (GDK_IS_DMABUF_TEXTURE (texture))
    {
      const GdkDmabuf *dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));
      GdkMemoryFormat format = gdk_texture_get_format (texture);
      gboolean external, supported_conversion = TRUE;
      GLuint tex_id[3];
      GskGpuConversion conv;
      EGLint color_space_hint, range_hint;
      gsize n;

      /* First try single-image import */
      if (gdk_memory_format_get_dmabuf_yuv_fourcc (format) == dmabuf->fourcc)
        {
          conv = gsk_gpu_color_state_get_conversion (gdk_texture_get_color_state (texture));

          range_hint = EGL_YUV_NARROW_RANGE_EXT;
          switch (conv)
            {
            case GSK_GPU_CONVERSION_NONE:
            case GSK_GPU_CONVERSION_SRGB:
              GDK_DEBUG (DMABUF, "EGL cannot import YUV dmabufs of colorstate %s",
                         gdk_color_state_get_name (gdk_texture_get_color_state (texture)));
              /* no hints for these */
              supported_conversion = FALSE;
              break;

            case GSK_GPU_CONVERSION_BT601:
              range_hint = EGL_YUV_FULL_RANGE_EXT;
              G_GNUC_FALLTHROUGH;
            case GSK_GPU_CONVERSION_BT601_NARROW:
              color_space_hint = EGL_ITU_REC601_EXT;
              break;

            case GSK_GPU_CONVERSION_BT709:
              range_hint = EGL_YUV_FULL_RANGE_EXT;
              G_GNUC_FALLTHROUGH;
            case GSK_GPU_CONVERSION_BT709_NARROW:
              color_space_hint = EGL_ITU_REC709_EXT;
              break;

            case GSK_GPU_CONVERSION_BT2020:
              range_hint = EGL_YUV_FULL_RANGE_EXT;
              G_GNUC_FALLTHROUGH;
            case GSK_GPU_CONVERSION_BT2020_NARROW:
              color_space_hint = EGL_ITU_REC2020_EXT;
              break;

            default:
              g_assert_not_reached ();
              supported_conversion = FALSE;
              break;
            }
        }
      else if (gdk_memory_format_get_dmabuf_rgb_fourcc (format) == dmabuf->fourcc)
        {
          conv = GSK_GPU_CONVERSION_NONE;
          color_space_hint = 0;
          range_hint = 0;
        }
      else
        {
          supported_conversion = FALSE;
        }

      if (supported_conversion)
        {
          tex_id[0] = gdk_dmabuf_egl_import_dmabuf (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                                    gdk_texture_get_width (texture),
                                                    gdk_texture_get_height (texture),
                                                    dmabuf,
                                                    color_space_hint,
                                                    range_hint,
                                                    &external);
          if (tex_id[0])
            {
              return gsk_gl_image_new_for_texture (GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                   texture,
                                                   1,
                                                   tex_id,
                                                   NULL,
                                                   0,
                                                   TRUE,
                                                   (external ? GSK_GPU_IMAGE_EXTERNAL : 0),
                                                   conv);
            }
        }

      /* Then try multi-image import */
      n = gdk_dmabuf_egl_import_dmabuf_multiplane (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                                   gdk_texture_get_width (texture),
                                                   gdk_texture_get_height (texture),
                                                   dmabuf,
                                                   tex_id);
      if (n > 0)
        {
          return gsk_gl_image_new_for_texture (GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame)),
                                               texture,
                                               n,
                                               tex_id,
                                               NULL,
                                               0,
                                               TRUE,
                                               0,
                                               GSK_GPU_CONVERSION_NONE);
        }
    }
#endif  /* HAVE_DMABUF && HAVE_EGL */
#ifdef GDK_WINDOWING_WIN32
  else if (GDK_IS_D3D12_TEXTURE (texture))
    {
      guint tex_id, mem_id, semaphore_id;

      tex_id = gdk_d3d12_texture_import_gl (GDK_D3D12_TEXTURE (texture),
                                            GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                            &mem_id,
                                            &semaphore_id);
      if (tex_id)
        {
          return gsk_gl_image_new_for_texture (GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame)),
                                               texture,
                                               1,
                                               &tex_id,
                                               &mem_id,
                                               semaphore_id,
                                               TRUE,
                                               0,
                                               GSK_GPU_CONVERSION_NONE);
        }
    }
#endif

  return GSK_GPU_FRAME_CLASS (gsk_gl_frame_parent_class)->upload_texture (frame, with_mipmap, texture);
}

static GskGpuBuffer *
gsk_gl_frame_create_vertex_buffer (GskGpuFrame *frame,
                                   gsize        size)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  /* We could also reassign them all to the new buffer here?
   * Is that faster?
   */
  g_hash_table_remove_all (self->vaos);

  if (gdk_gl_context_has_feature (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                  GDK_GL_FEATURE_BUFFER_STORAGE))
    return gsk_gl_mapped_buffer_new (GL_ARRAY_BUFFER, size);
  else
    return gsk_gl_copied_buffer_new (GL_ARRAY_BUFFER, size);
}

static GskGpuBuffer *
gsk_gl_frame_create_globals_buffer (GskGpuFrame *frame,
                                    gsize        size)
{
  if (gdk_gl_context_has_feature (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                  GDK_GL_FEATURE_BUFFER_STORAGE))
    return gsk_gl_mapped_buffer_new (GL_UNIFORM_BUFFER, size);
  else
    return gsk_gl_copied_buffer_new (GL_UNIFORM_BUFFER, size);
}

static GskGpuBuffer *
gsk_gl_frame_create_storage_buffer (GskGpuFrame *frame,
                                    gsize        size)
{
  if (gdk_gl_context_has_feature (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                  GDK_GL_FEATURE_BUFFER_STORAGE))
    return gsk_gl_mapped_buffer_new (GL_UNIFORM_BUFFER, size);
  else
    return gsk_gl_copied_buffer_new (GL_UNIFORM_BUFFER, size);
}

static void
gsk_gl_frame_write_texture_vertex_data (GskGpuFrame    *self,
                                        guchar         *data,
                                        GskGpuImage   **images,
                                        GskGpuSampler  *samplers,
                                        gsize           n_images)
{
}

static void
gsk_gl_frame_submit (GskGpuFrame       *frame,
                     GskRenderPassType  pass_type,
                     GskGpuBuffer      *vertex_buffer,
                     GskGpuBuffer      *globals_buffer,
                     GskGpuOp          *op)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);
  GskGLCommandState state = {
    /* rest is 0 */
    .current_samplers = { GSK_GPU_SAMPLER_N_SAMPLERS, GSK_GPU_SAMPLER_N_SAMPLERS },
    .globals = globals_buffer,
  };

  glEnable (GL_SCISSOR_TEST);

  glDisable (GL_DEPTH_TEST);
  glEnable (GL_BLEND);

  if (vertex_buffer)
    gsk_gl_buffer_bind (GSK_GL_BUFFER (vertex_buffer));

  while (op)
    {
      op = gsk_gpu_op_gl_command (op, frame, &state);
    }

  self->sync = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

static void
gsk_gl_frame_finalize (GObject *object)
{
  GskGLFrame *self = GSK_GL_FRAME (object);

  g_hash_table_unref (self->vaos);

  G_OBJECT_CLASS (gsk_gl_frame_parent_class)->finalize (object);
}

static void
gsk_gl_frame_class_init (GskGLFrameClass *klass)
{
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_frame_class->is_busy = gsk_gl_frame_is_busy;
  gpu_frame_class->wait = gsk_gl_frame_wait;
  gpu_frame_class->cleanup = gsk_gl_frame_cleanup;
  gpu_frame_class->upload_texture = gsk_gl_frame_upload_texture;
  gpu_frame_class->create_vertex_buffer = gsk_gl_frame_create_vertex_buffer;
  gpu_frame_class->create_globals_buffer = gsk_gl_frame_create_globals_buffer;
  gpu_frame_class->create_storage_buffer = gsk_gl_frame_create_storage_buffer;
  gpu_frame_class->write_texture_vertex_data = gsk_gl_frame_write_texture_vertex_data;
  gpu_frame_class->submit = gsk_gl_frame_submit;

  object_class->finalize = gsk_gl_frame_finalize;
}

static void
free_vao (gpointer vao)
{
  glDeleteVertexArrays (1, (GLuint[1]) { GPOINTER_TO_UINT (vao) });
}

static void
gsk_gl_frame_init (GskGLFrame *self)
{
  self->vaos = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free_vao);
}

void
gsk_gl_frame_use_program (GskGLFrame                *self,
                          const GskGpuShaderOpClass *op_class,
                          GskGpuShaderFlags          flags,
                          GskGpuColorStates          color_states,
                          guint32                    variation)
{
  GLuint vao;

  gsk_gl_device_use_program (GSK_GL_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self))),
                             op_class,
                             flags,
                             color_states,
                             variation);

  vao = GPOINTER_TO_UINT (g_hash_table_lookup (self->vaos, op_class));
  if (vao)
    {
      glBindVertexArray (vao);
      return;
    }
  glGenVertexArrays (1, &vao);
  glBindVertexArray (vao);
  op_class->setup_vao (0);

  g_hash_table_insert (self->vaos, (gpointer) op_class, GUINT_TO_POINTER (vao));
}


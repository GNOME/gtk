#include "config.h"

#include "gskglframeprivate.h"

#include "gskgpuglobalsopprivate.h"
#include "gskgpuopprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskglbufferprivate.h"
#include "gskgldescriptorsprivate.h"
#include "gskgldeviceprivate.h"

struct _GskGLFrame
{
  GskGpuFrame parent_instance;

  GLuint globals_buffer_id;
  guint next_texture_slot;

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
  return FALSE;
}

static void
gsk_gl_frame_setup (GskGpuFrame *frame)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  glGenBuffers (1, &self->globals_buffer_id);
}

static void
gsk_gl_frame_cleanup (GskGpuFrame *frame)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  self->next_texture_slot = 0;

  GSK_GPU_FRAME_CLASS (gsk_gl_frame_parent_class)->cleanup (frame);
}

static GskGpuDescriptors *
gsk_gl_frame_create_descriptors (GskGpuFrame *frame)
{
  return GSK_GPU_DESCRIPTORS (gsk_gl_descriptors_new (GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame))));
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

  return gsk_gl_buffer_new (GL_ARRAY_BUFFER, size, GL_WRITE_ONLY);
}

static GskGpuBuffer *
gsk_gl_frame_create_storage_buffer (GskGpuFrame *frame,
                                    gsize        size)
{
  return gsk_gl_buffer_new (GL_UNIFORM_BUFFER, size, GL_WRITE_ONLY);
}

static void
gsk_gl_frame_submit (GskGpuFrame  *frame,
                     GskGpuBuffer *vertex_buffer,
                     GskGpuBuffer *storage_buffer,
                     GskGpuOp     *op)
{
  GskGLFrame *self = GSK_GL_FRAME (frame);

  glEnable (GL_SCISSOR_TEST);

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LEQUAL);

  /* Pre-multiplied alpha */
  glEnable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquation (GL_FUNC_ADD);

  if (vertex_buffer)
    gsk_gl_buffer_bind (GSK_GL_BUFFER (vertex_buffer));
  if (storage_buffer)
    gsk_gl_buffer_bind_base (GSK_GL_BUFFER (storage_buffer), 1);
  /* The globals buffer must be the last bound buffer,
   * the globsals op relies on that. */
  glBindBufferBase (GL_UNIFORM_BUFFER, 0, self->globals_buffer_id);
  glBufferData (GL_UNIFORM_BUFFER,
                sizeof (GskGpuGlobalsInstance),
                NULL,
                GL_STREAM_DRAW);

  while (op)
    {
      op = gsk_gpu_op_gl_command (op, frame, 0);
    }
}

static void
gsk_gl_frame_finalize (GObject *object)
{
  GskGLFrame *self = GSK_GL_FRAME (object);

  g_hash_table_unref (self->vaos);
  glDeleteBuffers (1, &self->globals_buffer_id);

  G_OBJECT_CLASS (gsk_gl_frame_parent_class)->finalize (object);
}

static void
gsk_gl_frame_class_init (GskGLFrameClass *klass)
{
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_frame_class->is_busy = gsk_gl_frame_is_busy;
  gpu_frame_class->setup = gsk_gl_frame_setup;
  gpu_frame_class->cleanup = gsk_gl_frame_cleanup;
  gpu_frame_class->create_descriptors = gsk_gl_frame_create_descriptors;
  gpu_frame_class->create_vertex_buffer = gsk_gl_frame_create_vertex_buffer;
  gpu_frame_class->create_storage_buffer = gsk_gl_frame_create_storage_buffer;
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
                          GskGpuShaderClip           clip)
{
  GLuint vao;

  gsk_gl_device_use_program (GSK_GL_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self))),
                             op_class,
                             clip);

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


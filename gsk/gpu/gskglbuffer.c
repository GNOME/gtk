#include "config.h"

#include <gdk/gdkprofilerprivate.h>

#include "gskglbufferprivate.h"

struct _GskGLBuffer
{
  GskGpuBuffer parent_instance;

  GLenum target;
  GLuint buffer_id;
  GLenum access;
  guchar *data;
};

G_DEFINE_TYPE (GskGLBuffer, gsk_gl_buffer, GSK_TYPE_GPU_BUFFER)

static guint profiler_buffer_uploads_id;
static gint64 profiler_buffer_uploads;

static void
gsk_gl_buffer_finalize (GObject *object)
{
  GskGLBuffer *self = GSK_GL_BUFFER (object);

  g_free (self->data);
  glDeleteBuffers (1, &self->buffer_id);

  G_OBJECT_CLASS (gsk_gl_buffer_parent_class)->finalize (object);
}

static guchar *
gsk_gl_buffer_map (GskGpuBuffer *buffer)
{
  GskGLBuffer *self = GSK_GL_BUFFER (buffer);

  return self->data;
}

static void
gsk_gl_buffer_unmap (GskGpuBuffer *buffer,
                     gsize         used)
{
  GskGLBuffer *self = GSK_GL_BUFFER (buffer);

  if (used == 0)
    return;

  gsk_gl_buffer_bind (self);

  profiler_buffer_uploads += used;
  glBufferSubData (self->target, 0, used, self->data);
  gdk_profiler_set_int_counter (profiler_buffer_uploads_id, profiler_buffer_uploads);
}

static void
gsk_gl_buffer_class_init (GskGLBufferClass *klass)
{
  GskGpuBufferClass *buffer_class = GSK_GPU_BUFFER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  buffer_class->map = gsk_gl_buffer_map;
  buffer_class->unmap = gsk_gl_buffer_unmap;

  gobject_class->finalize = gsk_gl_buffer_finalize;

  profiler_buffer_uploads_id = gdk_profiler_define_int_counter ("ngl-buffer-uploads", "Number of bytes uploaded to GPU");
}

static void
gsk_gl_buffer_init (GskGLBuffer *self)
{
}

GskGpuBuffer *
gsk_gl_buffer_new (GLenum target,
                   gsize  size,
                   GLenum access)
{
  GskGLBuffer *self;

  self = g_object_new (GSK_TYPE_GL_BUFFER, NULL);

  gsk_gpu_buffer_setup (GSK_GPU_BUFFER (self), size);

  self->target = target;
  self->access = access;

  glGenBuffers (1, &self->buffer_id);
  glBindBuffer (target, self->buffer_id);
  glBufferData (target, size, NULL, GL_STATIC_DRAW);
  self->data = malloc (size);

  return GSK_GPU_BUFFER (self);
}

void
gsk_gl_buffer_bind (GskGLBuffer *self)
{
  glBindBuffer (self->target, self->buffer_id);
}

void
gsk_gl_buffer_bind_base (GskGLBuffer *self,
                         GLuint       index)
{
  glBindBufferBase (self->target, index, self->buffer_id);
}


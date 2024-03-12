#include "config.h"

#include "gskglmapbufferprivate.h"

struct _GskGLMapBuffer
{
  GskGpuBuffer parent_instance;

  GLenum target;
  GLuint buffer_id;
  GLenum access;
  gsize size;
};

G_DEFINE_TYPE (GskGLMapBuffer, gsk_gl_map_buffer, GSK_TYPE_GPU_BUFFER)

static void
gsk_gl_map_buffer_finalize (GObject *object)
{
  GskGLMapBuffer *self = GSK_GL_MAP_BUFFER (object);

  glDeleteBuffers (1, &self->buffer_id);

  G_OBJECT_CLASS (gsk_gl_map_buffer_parent_class)->finalize (object);
}

static guchar *
gsk_gl_map_buffer_map (GskGpuBuffer *buffer)
{
  GskGLMapBuffer *self = GSK_GL_MAP_BUFFER (buffer);
  int flags;

  gsk_gl_map_buffer_bind (self);

  switch (self->access)
    {
    case GL_READ_ONLY:
      flags = GL_MAP_READ_BIT;
      break;
    case GL_WRITE_ONLY:
      flags = GL_MAP_WRITE_BIT;
      break;
    case GL_READ_WRITE:
      flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
      break;
    default:
      g_assert_not_reached ();
    }

  return glMapBufferRange (self->target, 0, self->size, flags);
}

static void
gsk_gl_map_buffer_unmap (GskGpuBuffer *buffer,
                         gsize         used)
{
  GskGLMapBuffer *self = GSK_GL_MAP_BUFFER (buffer);

  if (!glUnmapBuffer (self->target))
    g_warning ("glUnmapBuffer failed");
}

static void
gsk_gl_map_buffer_class_init (GskGLMapBufferClass *klass)
{
  GskGpuBufferClass *buffer_class = GSK_GPU_BUFFER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  buffer_class->map = gsk_gl_map_buffer_map;
  buffer_class->unmap = gsk_gl_map_buffer_unmap;

  gobject_class->finalize = gsk_gl_map_buffer_finalize;
}

static void
gsk_gl_map_buffer_init (GskGLMapBuffer *self)
{
}

GskGpuBuffer *
gsk_gl_map_buffer_new (GLenum target,
                       gsize  size,
                       GLenum access)
{
  GskGLMapBuffer *self;

  self = g_object_new (GSK_TYPE_GL_MAP_BUFFER, NULL);

  gsk_gpu_buffer_setup (GSK_GPU_BUFFER (self), size);

  self->target = target;
  self->access = access;
  self->size = size;

  glGenBuffers (1, &self->buffer_id);
  glBindBuffer (target, self->buffer_id);
  glBufferData (target, size, NULL, GL_STATIC_DRAW);

  return GSK_GPU_BUFFER (self);
}

void
gsk_gl_map_buffer_bind (GskGLMapBuffer *self)
{
  glBindBuffer (self->target, self->buffer_id);
}

void
gsk_gl_map_buffer_bind_base (GskGLMapBuffer *self,
                             GLuint          index)
{
  glBindBufferBase (self->target, index, self->buffer_id);
}

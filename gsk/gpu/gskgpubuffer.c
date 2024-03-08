#include "config.h"

#include "gskgpubufferprivate.h"

typedef struct _GskGpuBufferPrivate GskGpuBufferPrivate;

struct _GskGpuBufferPrivate
{
  gsize size;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuBuffer, gsk_gpu_buffer, G_TYPE_OBJECT)

static void
gsk_gpu_buffer_class_init (GskGpuBufferClass *klass)
{
}

static void
gsk_gpu_buffer_init (GskGpuBuffer *self)
{
}

void
gsk_gpu_buffer_setup (GskGpuBuffer *self,
                      gsize         size)
{
  GskGpuBufferPrivate *priv = gsk_gpu_buffer_get_instance_private (self);

  priv->size = size;
}
                     
gsize
gsk_gpu_buffer_get_size (GskGpuBuffer *self)
{
  GskGpuBufferPrivate *priv = gsk_gpu_buffer_get_instance_private (self);

  return priv->size;
}

guchar *
gsk_gpu_buffer_map (GskGpuBuffer *self)
{
  return GSK_GPU_BUFFER_GET_CLASS (self)->map (self);
}

void
gsk_gpu_buffer_unmap (GskGpuBuffer *self,
                      gsize         size)
{
  GSK_GPU_BUFFER_GET_CLASS (self)->unmap (self, size);
}


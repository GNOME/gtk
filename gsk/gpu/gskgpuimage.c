#include "config.h"

#include "gskgpuimageprivate.h"

typedef struct _GskGpuImagePrivate GskGpuImagePrivate;

struct _GskGpuImagePrivate
{
  GdkMemoryFormat format;
  gsize width;
  gsize height;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuImage, gsk_gpu_image, G_TYPE_OBJECT)

static void
gsk_gpu_image_class_init (GskGpuImageClass *klass)
{
}

static void
gsk_gpu_image_init (GskGpuImage *self)
{
}

void
gsk_gpu_image_setup (GskGpuImage     *self,
                     GdkMemoryFormat  format,
                     gsize            width,
                     gsize            height)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  priv->format = format;
  priv->width = width;
  priv->height = height;
}
                     
GdkMemoryFormat
gsk_gpu_image_get_format (GskGpuImage *self)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  return priv->format;
}

gsize
gsk_gpu_image_get_width (GskGpuImage *self)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  return priv->width;
}

gsize
gsk_gpu_image_get_height (GskGpuImage *self)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  return priv->height;
}


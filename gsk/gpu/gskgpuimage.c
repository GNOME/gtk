#include "config.h"

#include "gskgpuimageprivate.h"

typedef struct _GskGpuImagePrivate GskGpuImagePrivate;

struct _GskGpuImagePrivate
{
  GskGpuImageFlags flags;
  GdkMemoryFormat format;
  gsize width;
  gsize height;
};

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuImage, gsk_gpu_image, G_TYPE_OBJECT)

static void
gsk_gpu_image_get_default_projection_matrix (GskGpuImage       *self,
                                             graphene_matrix_t *out_projection)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  graphene_matrix_init_ortho (out_projection,
                              0, priv->width,
                              0, priv->height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);
}

static void
gsk_gpu_image_class_init (GskGpuImageClass *klass)
{
  klass->get_projection_matrix = gsk_gpu_image_get_default_projection_matrix;
}

static void
gsk_gpu_image_init (GskGpuImage *self)
{
}

void
gsk_gpu_image_setup (GskGpuImage      *self,
                     GskGpuImageFlags  flags,
                     GdkMemoryFormat   format,
                     gsize             width,
                     gsize             height)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  priv->flags = flags;
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

GskGpuImageFlags
gsk_gpu_image_get_flags (GskGpuImage *self)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  return priv->flags;
}

void
gsk_gpu_image_set_flags (GskGpuImage      *self,
                         GskGpuImageFlags  flags)
{
  GskGpuImagePrivate *priv = gsk_gpu_image_get_instance_private (self);

  priv->flags |= flags;
}

void
gsk_gpu_image_get_projection_matrix (GskGpuImage       *self,
                                     graphene_matrix_t *out_projection)
{
  GSK_GPU_IMAGE_GET_CLASS (self)->get_projection_matrix (self, out_projection);
}

#include "config.h"

#include "gskgldescriptorsprivate.h"

#include "gskglimageprivate.h"

struct _GskGLDescriptors
{
  GskGpuDescriptors parent_instance;

  GskGLDevice *device;
};

G_DEFINE_TYPE (GskGLDescriptors, gsk_gl_descriptors, GSK_TYPE_GPU_DESCRIPTORS)

static void
gsk_gl_descriptors_finalize (GObject *object)
{
  GskGLDescriptors *self = GSK_GL_DESCRIPTORS (object);

  g_object_unref (self->device);

  G_OBJECT_CLASS (gsk_gl_descriptors_parent_class)->finalize (object);
}

static gboolean
gsk_gl_descriptors_add_image (GskGpuDescriptors *desc,
                              GskGpuImage       *image,
                              GskGpuSampler      sampler,
                              guint32           *out_descriptor)
{
  gsize used_texture_units;

  used_texture_units = gsk_gpu_descriptors_get_size (desc);
  if (used_texture_units >= 16)
    return FALSE;

  *out_descriptor = used_texture_units;
  return TRUE;
}

static void
gsk_gl_descriptors_class_init (GskGLDescriptorsClass *klass)
{
  GskGpuDescriptorsClass *descriptors_class = GSK_GPU_DESCRIPTORS_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_gl_descriptors_finalize;

  descriptors_class->add_image = gsk_gl_descriptors_add_image;
}

static void
gsk_gl_descriptors_init (GskGLDescriptors *self)
{
}

GskGpuDescriptors *
gsk_gl_descriptors_new (GskGLDevice *device)
{
  GskGLDescriptors *self;

  self = g_object_new (GSK_TYPE_GL_DESCRIPTORS, NULL);

  self->device = g_object_ref (device);

  return GSK_GPU_DESCRIPTORS (self);
}

void
gsk_gl_descriptors_use (GskGLDescriptors *self)
{
  GskGpuDescriptors *desc = GSK_GPU_DESCRIPTORS (self);
  gsize i;

  for (i = 0; i < gsk_gpu_descriptors_get_size (desc); i++)
    {
      glActiveTexture (GL_TEXTURE0 + i);
      gsk_gl_image_bind_texture (GSK_GL_IMAGE (gsk_gpu_descriptors_get_image (desc, i)));
      glBindSampler (i, gsk_gl_device_get_sampler_id (self->device, gsk_gpu_descriptors_get_sampler (desc, i)));
    }
}

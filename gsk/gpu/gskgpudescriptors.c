#include "config.h"

#include "gskgpudescriptorsprivate.h"

typedef struct _GskGpuImageEntry GskGpuImageEntry;

struct _GskGpuImageEntry
{
  GskGpuImage *image;
  GskGpuSampler sampler;
  guint32 descriptor;
};

static void
gsk_gpu_image_entry_clear (gpointer data)
{
  GskGpuImageEntry *entry = data;

  g_object_unref (entry->image);
}

#define GDK_ARRAY_NAME gsk_gpu_image_entries
#define GDK_ARRAY_TYPE_NAME GskGpuImageEntries
#define GDK_ARRAY_ELEMENT_TYPE GskGpuImageEntry
#define GDK_ARRAY_FREE_FUNC gsk_gpu_image_entry_clear
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

typedef struct _GskGpuDescriptorsPrivate GskGpuDescriptorsPrivate;

struct _GskGpuDescriptorsPrivate
{
  GskGpuImageEntries images;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuDescriptors, gsk_gpu_descriptors, G_TYPE_OBJECT)

static void
gsk_gpu_descriptors_finalize (GObject *object)
{
  GskGpuDescriptors *self = GSK_GPU_DESCRIPTORS (object);
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);

  gsk_gpu_image_entries_clear (&priv->images);

  G_OBJECT_CLASS (gsk_gpu_descriptors_parent_class)->finalize (object);
}

static void
gsk_gpu_descriptors_class_init (GskGpuDescriptorsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_gpu_descriptors_finalize;
}

static void
gsk_gpu_descriptors_init (GskGpuDescriptors *self)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);

  gsk_gpu_image_entries_init (&priv->images);
}

gsize
gsk_gpu_descriptors_get_n_images (GskGpuDescriptors *self)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);

  return gsk_gpu_image_entries_get_size (&priv->images);
}

void
gsk_gpu_descriptors_set_size (GskGpuDescriptors *self,
                              gsize              n_images)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);

  g_assert (n_images <= gsk_gpu_image_entries_get_size (&priv->images));
  gsk_gpu_image_entries_set_size (&priv->images, n_images);
}

GskGpuImage *
gsk_gpu_descriptors_get_image (GskGpuDescriptors *self,
                               gsize              id)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);
  const GskGpuImageEntry *entry = gsk_gpu_image_entries_get (&priv->images, id);

  return entry->image;
}

GskGpuSampler
gsk_gpu_descriptors_get_sampler (GskGpuDescriptors *self,
                                 gsize              id)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);
  const GskGpuImageEntry *entry = gsk_gpu_image_entries_get (&priv->images, id);

  return entry->sampler;
}

gsize
gsk_gpu_descriptors_find_image (GskGpuDescriptors *self,
                                guint32            descriptor)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);
  gsize i;

  for (i = 0; i < gsk_gpu_image_entries_get_size (&priv->images); i++)
    {
      const GskGpuImageEntry *entry = gsk_gpu_image_entries_get (&priv->images, i);

      if (entry->descriptor == descriptor)
        return i;
    }

  g_return_val_if_reached ((gsize) -1);
}

gboolean
gsk_gpu_descriptors_add_image (GskGpuDescriptors *self,
                               GskGpuImage       *image,
                               GskGpuSampler      sampler,
                               guint32           *out_descriptor)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);
  gsize i;
  guint32 descriptor;

  for (i = 0; i < gsk_gpu_image_entries_get_size (&priv->images); i++)
    {
      const GskGpuImageEntry *entry = gsk_gpu_image_entries_get (&priv->images, i);

      if (entry->image == image && entry->sampler == sampler)
        {
          *out_descriptor = entry->descriptor;
          return TRUE;
        }
    }

  if (!GSK_GPU_DESCRIPTORS_GET_CLASS (self)->add_image (self, image, sampler, &descriptor))
    return FALSE;

  gsk_gpu_image_entries_append (&priv->images,
                                &(GskGpuImageEntry) {
                                     .image = g_object_ref (image),
                                     .sampler = sampler,
                                     .descriptor = descriptor
                                });

  *out_descriptor = descriptor;

  return TRUE;
}


#include "config.h"

#include "gskgpudescriptorsprivate.h"

typedef struct _GskGpuImageEntry GskGpuImageEntry;
typedef struct _GskGpuBufferEntry GskGpuBufferEntry;

struct _GskGpuImageEntry
{
  GskGpuImage *image;
  GskGpuSampler sampler;
  guint32 descriptor;
};

struct _GskGpuBufferEntry
{
  GskGpuBuffer *buffer;
  guint32 descriptor;
};

static void
gsk_gpu_image_entry_clear (gpointer data)
{
  GskGpuImageEntry *entry = data;

  g_object_unref (entry->image);
}

static void
gsk_gpu_buffer_entry_clear (gpointer data)
{
  GskGpuBufferEntry *entry = data;

  g_object_unref (entry->buffer);
}

#define GDK_ARRAY_NAME gsk_gpu_image_entries
#define GDK_ARRAY_TYPE_NAME GskGpuImageEntries
#define GDK_ARRAY_ELEMENT_TYPE GskGpuImageEntry
#define GDK_ARRAY_FREE_FUNC gsk_gpu_image_entry_clear
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_gpu_buffer_entries
#define GDK_ARRAY_TYPE_NAME GskGpuBufferEntries
#define GDK_ARRAY_ELEMENT_TYPE GskGpuBufferEntry
#define GDK_ARRAY_FREE_FUNC gsk_gpu_buffer_entry_clear
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 4
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

struct _GskGpuDescriptorsPrivate
{
  GskGpuImageEntries images;
  GskGpuBufferEntries buffers;
};

static inline GskGpuDescriptorsPrivate *
gsk_gpu_descriptors_get_instance_private (GskGpuDescriptors *self)
{
  return (GskGpuDescriptorsPrivate *) (((guchar *)self) - sizeof (GskGpuDescriptorsPrivate));
}

/* Just for subclasses */
GskGpuDescriptors *
gsk_gpu_descriptors_new (GskGpuDescriptorsClass *desc_class,
                         gsize                   child_size)
{
  GskGpuDescriptors *self;
  GskGpuDescriptorsPrivate *priv;
  guchar *data;

  data = g_new0 (guchar, child_size + sizeof (GskGpuDescriptorsPrivate));

  priv = (GskGpuDescriptorsPrivate *) data;
  self = (GskGpuDescriptors *) (data + sizeof (GskGpuDescriptorsPrivate));

  self->ref_count = 1;
  self->desc_class = desc_class;

  gsk_gpu_image_entries_init (&priv->images);
  gsk_gpu_buffer_entries_init (&priv->buffers);

  return self;
}

gsize
gsk_gpu_descriptors_get_n_images (GskGpuDescriptors *self)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);

  return gsk_gpu_image_entries_get_size (&priv->images);
}

gsize
gsk_gpu_descriptors_get_n_buffers (GskGpuDescriptors *self)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);

  return gsk_gpu_buffer_entries_get_size (&priv->buffers);
}
void
gsk_gpu_descriptors_set_size (GskGpuDescriptors *self,
                              gsize              n_images,
                              gsize              n_buffers)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);

  g_assert (n_images <= gsk_gpu_image_entries_get_size (&priv->images));
  gsk_gpu_image_entries_set_size (&priv->images, n_images);

  g_assert (n_buffers <= gsk_gpu_buffer_entries_get_size (&priv->buffers));
  gsk_gpu_buffer_entries_set_size (&priv->buffers, n_buffers);
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

GskGpuBuffer *
gsk_gpu_descriptors_get_buffer (GskGpuDescriptors *self,
                               gsize              id)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);
  const GskGpuBufferEntry *entry = gsk_gpu_buffer_entries_get (&priv->buffers, id);

  return entry->buffer;
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

  if (!self->desc_class->add_image (self, image, sampler, &descriptor))
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

gboolean
gsk_gpu_descriptors_add_buffer (GskGpuDescriptors *self,
                                GskGpuBuffer      *buffer,
                                guint32           *out_descriptor)
{
  GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);
  gsize i;
  guint32 descriptor;

  for (i = 0; i < gsk_gpu_buffer_entries_get_size (&priv->buffers); i++)
    {
      const GskGpuBufferEntry *entry = gsk_gpu_buffer_entries_get (&priv->buffers, i);

      if (entry->buffer == buffer)
        {
          *out_descriptor = entry->descriptor;
          return TRUE;
        }
    }

  if (!self->desc_class->add_buffer (self, buffer, &descriptor))
    return FALSE;

  gsk_gpu_buffer_entries_append (&priv->buffers,
                                 &(GskGpuBufferEntry) {
                                      .buffer = g_object_ref (buffer),
                                      .descriptor = descriptor
                                 });

  *out_descriptor = descriptor;

  return TRUE;
}

GskGpuDescriptors *
gsk_gpu_descriptors_ref (GskGpuDescriptors *self)
{
  self->ref_count++;

  return self;
}

void
gsk_gpu_descriptors_unref (GskGpuDescriptors *self)
{
  self->ref_count--;

  if (self->ref_count == 0)
    {
      GskGpuDescriptorsPrivate *priv = gsk_gpu_descriptors_get_instance_private (self);

      self->desc_class->finalize (self);

      gsk_gpu_image_entries_clear (&priv->images);
      gsk_gpu_buffer_entries_clear (&priv->buffers);

      g_free (priv);
    }
}

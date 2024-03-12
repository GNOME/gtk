#include "config.h"

#include "gskgpudescriptorsprivate.h"

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

#define INCLUDE_IMPL 1

#define GDK_ARRAY_NAME gsk_gpu_image_entries
#define GDK_ARRAY_TYPE_NAME GskGpuImageEntries
#define GDK_ARRAY_ELEMENT_TYPE GskGpuImageEntry
#define GDK_ARRAY_FREE_FUNC gsk_gpu_image_entry_clear
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define INCLUDE_IMPL 1

#define GDK_ARRAY_NAME gsk_gpu_buffer_entries
#define GDK_ARRAY_TYPE_NAME GskGpuBufferEntries
#define GDK_ARRAY_ELEMENT_TYPE GskGpuBufferEntry
#define GDK_ARRAY_FREE_FUNC gsk_gpu_buffer_entry_clear
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 4
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

void
gsk_gpu_descriptors_finalize (GskGpuDescriptors *self)
{
  gsk_gpu_image_entries_clear (&self->images);
  gsk_gpu_buffer_entries_clear (&self->buffers);
}

void
gsk_gpu_descriptors_init (GskGpuDescriptors *self)
{
  gsk_gpu_image_entries_init (&self->images);
  gsk_gpu_buffer_entries_init (&self->buffers);
}

gsize
gsk_gpu_descriptors_get_n_images (GskGpuDescriptors *self)
{
  return gsk_gpu_image_entries_get_size (&self->images);
}

gsize
gsk_gpu_descriptors_get_n_buffers (GskGpuDescriptors *self)
{
  return gsk_gpu_buffer_entries_get_size (&self->buffers);
}
void
gsk_gpu_descriptors_set_size (GskGpuDescriptors *self,
                              gsize              n_images,
                              gsize              n_buffers)
{
  g_assert (n_images <= gsk_gpu_image_entries_get_size (&self->images));
  gsk_gpu_image_entries_set_size (&self->images, n_images);

  g_assert (n_buffers <= gsk_gpu_buffer_entries_get_size (&self->buffers));
  gsk_gpu_buffer_entries_set_size (&self->buffers, n_buffers);
}

GskGpuImage *
gsk_gpu_descriptors_get_image (GskGpuDescriptors *self,
                               gsize              id)
{
  const GskGpuImageEntry *entry = gsk_gpu_image_entries_get (&self->images, id);

  return entry->image;
}

GskGpuSampler
gsk_gpu_descriptors_get_sampler (GskGpuDescriptors *self,
                                 gsize              id)
{
  const GskGpuImageEntry *entry = gsk_gpu_image_entries_get (&self->images, id);

  return entry->sampler;
}

gsize
gsk_gpu_descriptors_find_image (GskGpuDescriptors *self,
                                guint32            descriptor)
{
  gsize i;

  for (i = 0; i < gsk_gpu_image_entries_get_size (&self->images); i++)
    {
      const GskGpuImageEntry *entry = gsk_gpu_image_entries_get (&self->images, i);

      if (entry->descriptor == descriptor)
        return i;
    }

  g_return_val_if_reached ((gsize) -1);
}

GskGpuBuffer *
gsk_gpu_descriptors_get_buffer (GskGpuDescriptors *self,
                               gsize              id)
{
  const GskGpuBufferEntry *entry = gsk_gpu_buffer_entries_get (&self->buffers, id);

  return entry->buffer;
}

gboolean
gsk_gpu_descriptors_add_image (GskGpuDescriptors *self,
                               GskGpuImage       *image,
                               GskGpuSampler      sampler,
                               guint32           *out_descriptor)
{
  gsize i;
  guint32 descriptor;

  for (i = 0; i < gsk_gpu_image_entries_get_size (&self->images); i++)
    {
      const GskGpuImageEntry *entry = gsk_gpu_image_entries_get (&self->images, i);

      if (entry->image == image && entry->sampler == sampler)
        {
          *out_descriptor = entry->descriptor;
          return TRUE;
        }
    }

  if (!self->desc_class->add_image (self, image, sampler, &descriptor))
    return FALSE;

  gsk_gpu_image_entries_append (&self->images,
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
  gsize i;
  guint32 descriptor;

  for (i = 0; i < gsk_gpu_buffer_entries_get_size (&self->buffers); i++)
    {
      const GskGpuBufferEntry *entry = gsk_gpu_buffer_entries_get (&self->buffers, i);

      if (entry->buffer == buffer)
        {
          *out_descriptor = entry->descriptor;
          return TRUE;
        }
    }

  if (!self->desc_class->add_buffer (self, buffer, &descriptor))
    return FALSE;

  gsk_gpu_buffer_entries_append (&self->buffers,
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
      self->desc_class->finalize (self);
      g_free (self);
    }
}

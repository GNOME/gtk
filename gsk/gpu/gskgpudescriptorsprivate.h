#pragma once

#include "gskgputypesprivate.h"

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

#define INCLUDE_DECL 1

#define GDK_ARRAY_NAME gsk_gpu_image_entries
#define GDK_ARRAY_TYPE_NAME GskGpuImageEntries
#define GDK_ARRAY_ELEMENT_TYPE GskGpuImageEntry
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 16
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define INCLUDE_DECL 1

#define GDK_ARRAY_NAME gsk_gpu_buffer_entries
#define GDK_ARRAY_TYPE_NAME GskGpuBufferEntries
#define GDK_ARRAY_ELEMENT_TYPE GskGpuBufferEntry
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 4
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

G_BEGIN_DECLS

typedef struct _GskGpuDescriptors GskGpuDescriptors;
typedef struct _GskGpuDescriptorsClass GskGpuDescriptorsClass;

#define GSK_GPU_DESCRIPTORS(d) ((GskGpuDescriptors *) (d))

struct _GskGpuDescriptors
{
  GskGpuDescriptorsClass *desc_class;

  int ref_count;

  GskGpuImageEntries images;
  GskGpuBufferEntries buffers;
};

struct _GskGpuDescriptorsClass
{
  void                  (* finalize)                                    (GskGpuDescriptors      *self);
  gboolean              (* add_image)                                   (GskGpuDescriptors      *self,
                                                                         GskGpuImage            *image,
                                                                         GskGpuSampler           sampler,
                                                                         guint32                *out_id);
  gboolean              (* add_buffer)                                  (GskGpuDescriptors      *self,
                                                                         GskGpuBuffer           *buffer,
                                                                         guint32                *out_id);
};

GskGpuDescriptors *     gsk_gpu_descriptors_ref                         (GskGpuDescriptors      *self);
void                    gsk_gpu_descriptors_unref                       (GskGpuDescriptors      *self);


gsize                   gsk_gpu_descriptors_get_n_images                (GskGpuDescriptors      *self);
gsize                   gsk_gpu_descriptors_get_n_buffers               (GskGpuDescriptors      *self);
void                    gsk_gpu_descriptors_set_size                    (GskGpuDescriptors      *self,
                                                                         gsize                   n_images,
                                                                         gsize                   n_buffers);
GskGpuImage *           gsk_gpu_descriptors_get_image                   (GskGpuDescriptors      *self,
                                                                         gsize                   id);
GskGpuSampler           gsk_gpu_descriptors_get_sampler                 (GskGpuDescriptors      *self,
                                                                         gsize                   id);
gsize                   gsk_gpu_descriptors_find_image                  (GskGpuDescriptors      *self,
                                                                         guint32                 descriptor);
GskGpuBuffer *          gsk_gpu_descriptors_get_buffer                  (GskGpuDescriptors      *self,
                                                                         gsize                   id);

gboolean                gsk_gpu_descriptors_add_image                   (GskGpuDescriptors      *self,
                                                                         GskGpuImage            *image,
                                                                         GskGpuSampler           sampler,
                                                                         guint32                *out_descriptor);
gboolean                gsk_gpu_descriptors_add_buffer                  (GskGpuDescriptors      *self,
                                                                         GskGpuBuffer           *buffer,
                                                                         guint32                *out_descriptor);

void gsk_gpu_descriptors_finalize (GskGpuDescriptors *self);
void gsk_gpu_descriptors_init     (GskGpuDescriptors *self);

G_END_DECLS

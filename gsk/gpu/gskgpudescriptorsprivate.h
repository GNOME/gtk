#pragma once

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

typedef struct _GskGpuDescriptors GskGpuDescriptors;
typedef struct _GskGpuDescriptorsClass GskGpuDescriptorsClass;
typedef struct _GskGpuDescriptorsPrivate GskGpuDescriptorsPrivate;

#define GSK_GPU_DESCRIPTORS(d) ((GskGpuDescriptors *) (d))

struct _GskGpuDescriptors
{
  GskGpuDescriptorsClass *desc_class;
  int ref_count;

  GskGpuDescriptorsPrivate *priv;
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

GskGpuDescriptors      *gsk_gpu_descriptors_new                         (GskGpuDescriptorsClass *desc_class,
                                                                         gsize                   child_size);


G_END_DECLS

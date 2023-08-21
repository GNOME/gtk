#pragma once

#include "gskgpurenderer.h"
#include "gskgputypesprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GPU_FRAME         (gsk_gpu_frame_get_type ())
#define GSK_GPU_FRAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSK_TYPE_GPU_FRAME, GskGpuFrame))
#define GSK_GPU_FRAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSK_TYPE_GPU_FRAME, GskGpuFrameClass))
#define GSK_IS_GPU_FRAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSK_TYPE_GPU_FRAME))
#define GSK_IS_GPU_FRAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSK_TYPE_GPU_FRAME))
#define GSK_GPU_FRAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSK_TYPE_GPU_FRAME, GskGpuFrameClass))

typedef struct _GskGpuFrameClass GskGpuFrameClass;

struct _GskGpuFrame
{
  GObject parent_instance;
};

struct _GskGpuFrameClass
{
  GObjectClass parent_class;

  gboolean              (* is_busy)                                     (GskGpuFrame            *self);
  void                  (* setup)                                       (GskGpuFrame            *self);
  void                  (* cleanup)                                     (GskGpuFrame            *self);
  guint32               (* get_image_descriptor)                        (GskGpuFrame            *self,
                                                                         GskGpuImage            *image,
                                                                         GskGpuSampler           sampler);
  GskGpuBuffer *        (* create_vertex_buffer)                        (GskGpuFrame            *self,
                                                                         gsize                   size);
  void                  (* submit)                                      (GskGpuFrame            *self,
                                                                         GskGpuBuffer           *vertex_buffer,
                                                                         GskGpuOp               *op);
};

GType                   gsk_gpu_frame_get_type                          (void) G_GNUC_CONST;


void                    gsk_gpu_frame_setup                             (GskGpuFrame            *self,
                                                                         GskGpuRenderer         *renderer,
                                                                         GskGpuDevice           *device);

GdkDrawContext *        gsk_gpu_frame_get_context                       (GskGpuFrame            *self) G_GNUC_PURE;
GskGpuDevice *          gsk_gpu_frame_get_device                        (GskGpuFrame            *self) G_GNUC_PURE;

gpointer                gsk_gpu_frame_alloc_op                          (GskGpuFrame            *self,
                                                                         gsize                   size);
gsize                   gsk_gpu_frame_reserve_vertex_data               (GskGpuFrame            *self,
                                                                         gsize                   size);
guchar *                gsk_gpu_frame_get_vertex_data                   (GskGpuFrame            *self,
                                                                         gsize                   offset);
guint32                 gsk_gpu_frame_get_image_descriptor              (GskGpuFrame            *self,
                                                                         GskGpuImage            *image,
                                                                         GskGpuSampler           sampler);

gboolean                gsk_gpu_frame_is_busy                           (GskGpuFrame            *self);

void                    gsk_gpu_frame_render                            (GskGpuFrame            *self,
                                                                         GskGpuImage            *target,
                                                                         const cairo_region_t   *region,
                                                                         GskRenderNode          *node,
                                                                         const graphene_rect_t  *viewport,
                                                                         GdkTexture            **texture);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuFrame, g_object_unref)

G_END_DECLS

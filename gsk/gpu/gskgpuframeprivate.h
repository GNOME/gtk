#pragma once

#include "gskgpurenderer.h"
#include "gskgputypesprivate.h"
#include "gskgpubufferprivate.h"

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

  GskGpuBuffer *vertex_buffer;
  gsize vertex_buffer_used;
  guchar *vertex_buffer_data;
};

struct _GskGpuFrameClass
{
  GObjectClass parent_class;

  gboolean              (* is_busy)                                     (GskGpuFrame            *self);
  void                  (* setup)                                       (GskGpuFrame            *self);
  void                  (* cleanup)                                     (GskGpuFrame            *self);
  GskGpuImage *         (* upload_texture)                              (GskGpuFrame            *self,
                                                                         gboolean                with_mipmap,
                                                                         GdkTexture             *texture);
  GskGpuDescriptors *   (* create_descriptors)                          (GskGpuFrame            *self);
  GskGpuBuffer *        (* create_vertex_buffer)                        (GskGpuFrame            *self,
                                                                         gsize                   size);
  GskGpuBuffer *        (* create_storage_buffer)                       (GskGpuFrame            *self,
                                                                         gsize                   size);
  void                  (* submit)                                      (GskGpuFrame            *self,
                                                                         GskGpuBuffer           *vertex_buffer,
                                                                         GskGpuOp               *op);
};

GType                   gsk_gpu_frame_get_type                          (void) G_GNUC_CONST;


void                    gsk_gpu_frame_setup                             (GskGpuFrame            *self,
                                                                         GskGpuRenderer         *renderer,
                                                                         GskGpuDevice           *device,
                                                                         GskGpuOptimizations     optimizations);

GdkDrawContext *        gsk_gpu_frame_get_context                       (GskGpuFrame            *self) G_GNUC_PURE;
GskGpuDevice *          gsk_gpu_frame_get_device                        (GskGpuFrame            *self) G_GNUC_PURE;
gint64                  gsk_gpu_frame_get_timestamp                     (GskGpuFrame            *self) G_GNUC_PURE;
gboolean                gsk_gpu_frame_should_optimize                   (GskGpuFrame            *self,
                                                                         GskGpuOptimizations     optimization) G_GNUC_PURE;

gpointer                gsk_gpu_frame_alloc_op                          (GskGpuFrame            *self,
                                                                         gsize                   size);
GskGpuImage *           gsk_gpu_frame_upload_texture                    (GskGpuFrame            *self,
                                                                         gboolean                with_mipmap,
                                                                         GdkTexture             *texture);
GskGpuDescriptors *     gsk_gpu_frame_create_descriptors                (GskGpuFrame            *self);
gsize                  _gsk_gpu_frame_reserve_vertex_data               (GskGpuFrame            *self,
                                                                         gsize                   size);
GskGpuBuffer *          gsk_gpu_frame_write_storage_buffer              (GskGpuFrame            *self,
                                                                         const guchar           *data,
                                                                         gsize                   size,
                                                                         gsize                  *out_offset);

gboolean                gsk_gpu_frame_is_busy                           (GskGpuFrame            *self);

void                    gsk_gpu_frame_render                            (GskGpuFrame            *self,
                                                                         gint64                  timestamp,
                                                                         GskGpuImage            *target,
                                                                         const cairo_region_t   *region,
                                                                         GskRenderNode          *node,
                                                                         const graphene_rect_t  *viewport,
                                                                         GdkTexture            **texture);
void                    gsk_gpu_frame_download_texture                  (GskGpuFrame            *self,
                                                                         gint64                  timestamp,
                                                                         GdkTexture             *texture,
                                                                         GdkMemoryFormat         format,
                                                                         guchar                 *data,
                                                                         gsize                   stride);
GskGpuOp               *gsk_gpu_frame_get_last_op                       (GskGpuFrame            *self);

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

static inline gsize
gsk_gpu_frame_reserve_vertex_data (GskGpuFrame *self,
                                   gsize        size)
{
  if (self->vertex_buffer != NULL)
  {
    gsize size_needed = round_up (self->vertex_buffer_used, size) + size;

    if (gsk_gpu_buffer_get_size (self->vertex_buffer) >= size_needed)
      {
        self->vertex_buffer_used = size_needed;
        return size_needed - size;
      }
  }

  return _gsk_gpu_frame_reserve_vertex_data (self, size);
}

static inline guchar *
gsk_gpu_frame_get_vertex_data (GskGpuFrame *self,
                               gsize        offset)
{
  if G_UNLIKELY (self->vertex_buffer_data == NULL)
    self->vertex_buffer_data = gsk_gpu_buffer_map (self->vertex_buffer);

  return self->vertex_buffer_data + offset;
}


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuFrame, g_object_unref)

G_END_DECLS

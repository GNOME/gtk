#pragma once

#include "gskgputypesprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GskGpuBufferWriter GskGpuBufferWriter;

struct _GskGpuBufferWriter
{
  gpointer user_data;
  void                  (* ensure_size)                                 (GskGpuBufferWriter             *self,
                                                                         gsize                           size);
  gsize                 (* finish)                                      (GskGpuBufferWriter             *self,
                                                                         gboolean                        commit);

  guchar *data;
  gsize size;
  gsize allocated;
};

gsize                   gsk_gpu_buffer_writer_commit                    (GskGpuBufferWriter             *self);
void                    gsk_gpu_buffer_writer_abort                     (GskGpuBufferWriter             *self);

gsize                   gsk_gpu_buffer_writer_get_size                  (GskGpuBufferWriter             *self);
void                    gsk_gpu_buffer_writer_rewind                    (GskGpuBufferWriter             *self,
                                                                         gsize                           size);
void                    gsk_gpu_buffer_writer_ensure_size               (GskGpuBufferWriter             *self,
                                                                         gsize                           size);
void                    gsk_gpu_buffer_writer_append                    (GskGpuBufferWriter             *self,
                                                                         gsize                           align,
                                                                         const guchar                   *data,
                                                                         gsize                           size);

void                    gsk_gpu_buffer_writer_append_float              (GskGpuBufferWriter             *self,
                                                                         float                           f);
void                    gsk_gpu_buffer_writer_append_int                (GskGpuBufferWriter             *self,
                                                                         gint32                          i);
void                    gsk_gpu_buffer_writer_append_uint               (GskGpuBufferWriter             *self,
                                                                         guint32                         u);
void                    gsk_gpu_buffer_writer_append_matrix             (GskGpuBufferWriter             *self,
                                                                         const graphene_matrix_t        *matrix);
void                    gsk_gpu_buffer_writer_append_vec4               (GskGpuBufferWriter             *self,
                                                                         const graphene_vec4_t          *vec4);
void                    gsk_gpu_buffer_writer_append_rect               (GskGpuBufferWriter             *self,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset);
void                    gsk_gpu_buffer_writer_append_rgba               (GskGpuBufferWriter             *self,
                                                                         const GdkRGBA                  *rgba);

G_END_DECLS

#pragma once

#include "gskgpuopprivate.h"

#include "gsktypes.h"

G_BEGIN_DECLS

typedef void            (* GskGpuCairoFunc)                             (gpointer                        user_data,
                                                                         cairo_t                        *cr);
typedef void            (* GskGpuCairoPrintFunc)                        (gpointer                        user_data,
                                                                         GString                        *string);

GskGpuImage *           gsk_gpu_upload_texture_op_try                   (GskGpuFrame                    *frame,
                                                                         gboolean                        with_mipmap,
                                                                         guint                           lod_level,
                                                                         GskScalingFilter                lod_filter,
                                                                         GdkTexture                     *texture);

GskGpuImage *           gsk_gpu_upload_cairo_op                         (GskGpuFrame                    *frame,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport,
                                                                         GskGpuCairoFunc                 func,
                                                                         gpointer                        user_data,
                                                                         GDestroyNotify                  user_destroy);

void                    gsk_gpu_upload_cairo_into_op                    (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *image,
                                                                         const cairo_rectangle_int_t    *area,
                                                                         const graphene_rect_t          *viewport,
                                                                         GskGpuCairoFunc                 func,
                                                                         GskGpuCairoPrintFunc            print_func,
                                                                         gpointer                        user_data,
                                                                         GDestroyNotify                  user_destroy);


G_END_DECLS


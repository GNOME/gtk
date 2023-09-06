#pragma once

#include "gskgpuopprivate.h"

#include "gsktypes.h"

G_BEGIN_DECLS

GskGpuImage *           gsk_gpu_upload_texture_op                       (GskGpuFrame                    *frame,
                                                                         GdkTexture                     *texture);

GskGpuImage *           gsk_gpu_upload_cairo_op                         (GskGpuFrame                    *frame,
                                                                         GskRenderNode                  *node,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport);

void                    gsk_gpu_upload_glyph_op                         (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *image,
                                                                         PangoFont                      *font,
                                                                         PangoGlyph                      glyph,
                                                                         const cairo_rectangle_int_t    *area,
                                                                         float                           scale,
                                                                         const graphene_point_t         *origin);

G_END_DECLS


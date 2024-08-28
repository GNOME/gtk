#pragma once

#include "gskgputypesprivate.h"

#include "gsktypes.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_render_pass_begin_op                    (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *image,
                                                                         const cairo_rectangle_int_t    *area,
                                                                         GskGpuLoadOp                    load_op,
                                                                         float                           clear_color[4],
                                                                         GskRenderPassType               pass_type);
void                    gsk_gpu_render_pass_end_op                      (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *image,
                                                                         GskRenderPassType               pass_type);

G_END_DECLS


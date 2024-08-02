#pragma once

#include "gskgputypesprivate.h"
#include "gskgpucolorstatesprivate.h"

G_BEGIN_DECLS

void                    gsk_gpu_clear_op                                (GskGpuFrame                    *frame,
                                                                         const cairo_rectangle_int_t    *rect,
                                                                         const float                     color[4]);


G_END_DECLS


#pragma once

#include "gskgpubufferwriterprivate.h"

#include "gsktypes.h"

G_BEGIN_DECLS

typedef enum {
  GSK_GPU_PATTERN_COLOR,
} GskGpuPatternType;

gboolean                gsk_gpu_pattern_create_for_node                 (GskGpuBufferWriter             *writer,
                                                                         GskRenderNode                  *node);


G_END_DECLS


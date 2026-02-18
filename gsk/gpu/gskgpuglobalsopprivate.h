#pragma once

#include "gskgpuopprivate.h"

#include <gsk/gskroundedrect.h>
#include <graphene.h>

G_BEGIN_DECLS

struct _GskGpuGlobalsInstance
{
  float mvp[16];
  float scale[2];
  float color_volume[6];
  float clip_mask_rect[4];
  float clip[12];
  float padding[24];
};

/* GPUs often want 32bit alignment */
G_STATIC_ASSERT (sizeof (GskGpuGlobalsInstance) % 32 == 0);

void                    gsk_gpu_globals_op                              (GskGpuFrame                    *frame,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_matrix_t        *mvp,
                                                                         const graphene_rect_t          *clip_mask_rect,
                                                                         const GskRoundedRect           *clip);


G_END_DECLS


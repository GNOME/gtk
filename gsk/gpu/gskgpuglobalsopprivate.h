#pragma once

#include "gskgpuopprivate.h"

#include <gsk/gskroundedrect.h>
#include "gsk/gskscaleprivate.h"
#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GskGpuGlobalsInstance GskGpuGlobalsInstance;

struct _GskGpuGlobalsInstance
{
  float mvp[16];
  float clip[12];
  float scale[2];
};

void                    gsk_gpu_globals_op                              (GskGpuFrame                    *frame,
                                                                         const GskScale                 *scale,
                                                                         const graphene_matrix_t        *mvp,
                                                                         const GskRoundedRect           *clip);


G_END_DECLS


#pragma once

#include <gdk/gdk.h>
#include <gsk/gsktypes.h>
#include <graphene.h>

#include "gskgpuclipprivate.h"
#include "gskgputypesprivate.h"

G_BEGIN_DECLS

typedef enum {
  GSK_GPU_GLOBAL_MATRIX  = (1 << 0),
  GSK_GPU_GLOBAL_SCALE   = (1 << 1),
  GSK_GPU_GLOBAL_CLIP    = (1 << 2),
  GSK_GPU_GLOBAL_SCISSOR = (1 << 3),
  GSK_GPU_GLOBAL_BLEND   = (1 << 4),
} GskGpuGlobals;

struct _GskGpuRenderPass
{
  GskGpuFrame                   *frame;
  GdkColorState                 *ccs;
  cairo_rectangle_int_t          scissor;
  GskGpuBlend                    blend;
  graphene_point_t               offset;
  graphene_matrix_t              projection;
  graphene_vec2_t                scale;
  GskTransform                  *modelview;
  GskGpuClip                     clip;
  float                          opacity;

  GskGpuGlobals                  pending_globals;
};


G_END_DECLS


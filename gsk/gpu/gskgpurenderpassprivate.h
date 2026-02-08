#pragma once

#include <gdk/gdk.h>
#include <gsk/gsktypes.h>
#include <graphene.h>

#include "gskgpuclipprivate.h"
#include "gskgputypesprivate.h"
#include "gskgputransformprivate.h"

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
  GskGpuImage                   *target;
  GdkColorState                 *ccs;
  GskRenderPassType              pass_type;
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

typedef struct {
  graphene_point_t offset;
} GskGpuRenderPassTranslateStorage;

void                    gsk_gpu_render_pass_init                        (GskGpuRenderPass               *self,
                                                                         GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *target,
                                                                         GdkColorState                  *ccs,
                                                                         GskRenderPassType               pass_type,
                                                                         GskGpuLoadOp                    load_op,
                                                                         float                           clear_color[4],
                                                                         const cairo_rectangle_int_t    *clip,
                                                                         const graphene_rect_t          *viewport);

void                    gsk_gpu_render_pass_finish                      (GskGpuRenderPass               *self);

void                    gsk_gpu_render_pass_set_transform               (GskGpuRenderPass               *self,
                                                                         GskGpuTransform                *transform);
void                    gsk_gpu_render_pass_push_translate              (GskGpuRenderPass               *self,
                                                                         const graphene_point_t         *offset,
                                                                         GskGpuRenderPassTranslateStorage *storage);
void                    gsk_gpu_render_pass_pop_translate               (GskGpuRenderPass               *self,
                                                                         GskGpuRenderPassTranslateStorage *storage);

G_END_DECLS


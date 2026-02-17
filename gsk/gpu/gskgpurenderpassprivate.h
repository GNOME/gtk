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
  GskGpuBlend blend;
} GskGpuRenderPassBlendStorage;

typedef struct {
  float opacity;
} GskGpuRenderPassOpacityStorage;

typedef struct {
  GskTransform *modelview;
  graphene_vec2_t scale;
  graphene_point_t offset;
  GskGpuClip clip;
  GskGpuGlobals modified;
} GskGpuRenderPassTransformStorage;

typedef struct {
  graphene_point_t offset;
} GskGpuRenderPassTranslateStorage;

typedef struct {
  GskGpuClip clip;
  cairo_rectangle_int_t scissor;
  guint modified;
} GskGpuRenderPassClipStorage;

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

void                    gsk_gpu_render_pass_prepare_shader              (GskGpuRenderPass               *self);

gboolean                gsk_gpu_render_pass_device_to_user              (GskGpuRenderPass               *self,
                                                                         const cairo_rectangle_int_t    *device,
                                                                         graphene_rect_t                *user) G_GNUC_WARN_UNUSED_RESULT;
gboolean                gsk_gpu_render_pass_user_to_device_shrink       (GskGpuRenderPass               *self,
                                                                         const graphene_rect_t          *user,
                                                                         cairo_rectangle_int_t          *device) G_GNUC_WARN_UNUSED_RESULT;
gboolean                gsk_gpu_render_pass_user_to_device_exact        (GskGpuRenderPass               *self,
                                                                         const graphene_rect_t          *user,
                                                                         cairo_rectangle_int_t          *device) G_GNUC_WARN_UNUSED_RESULT;

void                    gsk_gpu_render_pass_push_blend                  (GskGpuRenderPass               *self,
                                                                         GskGpuBlend                     blend,
                                                                         GskGpuRenderPassBlendStorage   *storage);
void                    gsk_gpu_render_pass_pop_blend                   (GskGpuRenderPass               *self,
                                                                         GskGpuRenderPassBlendStorage   *storage);

void                    gsk_gpu_render_pass_push_opacity                (GskGpuRenderPass               *self,
                                                                         float                           opacity,
                                                                         GskGpuRenderPassOpacityStorage *storage);
void                    gsk_gpu_render_pass_pop_opacity                 (GskGpuRenderPass               *self,
                                                                         GskGpuRenderPassOpacityStorage *storage);
gboolean                gsk_gpu_render_pass_has_opacity                 (GskGpuRenderPass               *self);

void                    gsk_gpu_render_pass_set_transform               (GskGpuRenderPass               *self,
                                                                         GskGpuTransform                *transform);
gboolean                gsk_gpu_render_pass_push_transform              (GskGpuRenderPass               *self,
                                                                         GskTransform                   *transform,
                                                                         const graphene_rect_t          *bounds,
                                                                         const graphene_rect_t          *child_bounds,
                                                                         GskGpuRenderPassTransformStorage *storage);
void                    gsk_gpu_render_pass_pop_transform               (GskGpuRenderPass               *self,
                                                                         GskGpuRenderPassTransformStorage *storage);
void                    gsk_gpu_render_pass_push_translate              (GskGpuRenderPass               *self,
                                                                         const graphene_point_t         *offset,
                                                                         GskGpuRenderPassTranslateStorage *storage);
void                    gsk_gpu_render_pass_pop_translate               (GskGpuRenderPass               *self,
                                                                         GskGpuRenderPassTranslateStorage *storage);

gboolean                gsk_gpu_render_pass_is_all_clipped              (GskGpuRenderPass                 *self);
gboolean                gsk_gpu_render_pass_get_clip_bounds             (GskGpuRenderPass                 *self,
                                                                         graphene_rect_t                  *out_bounds) G_GNUC_WARN_UNUSED_RESULT;

gboolean                gsk_gpu_render_pass_push_clip_rect              (GskGpuRenderPass                 *self,
                                                                         const graphene_rect_t            *clip,
                                                                         GskGpuRenderPassClipStorage      *storage);
void                    gsk_gpu_render_pass_pop_clip_rect               (GskGpuRenderPass                 *self,
                                                                         GskGpuRenderPassClipStorage      *storage);
gboolean                gsk_gpu_render_pass_push_clip_rounded           (GskGpuRenderPass                 *self,
                                                                         const GskRoundedRect             *clip,
                                                                         GskGpuRenderPassClipStorage      *storage);
void                    gsk_gpu_render_pass_pop_clip_rounded            (GskGpuRenderPass                 *self,
                                                                         GskGpuRenderPassClipStorage      *storage);
void                    gsk_gpu_render_pass_push_clip_device_rect       (GskGpuRenderPass                 *self,
                                                                         const cairo_rectangle_int_t      *clip,
                                                                         GskGpuRenderPassClipStorage      *storage);
void                    gsk_gpu_render_pass_pop_clip_device_rect        (GskGpuRenderPass                 *self,
                                                                         GskGpuRenderPassClipStorage      *storage);

G_END_DECLS


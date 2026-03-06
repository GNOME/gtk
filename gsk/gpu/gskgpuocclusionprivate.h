#pragma once

#include <gdk/gdk.h>
#include <gsk/gsktypes.h>
#include <graphene.h>

#include "gskgputypesprivate.h"
#include "gskgpurenderpassprivate.h"
#include "gskgputransformprivate.h"

#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

void                    gsk_gpu_occlusion_render_node                   (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *target,
                                                                         GdkColorState                  *target_color_state,
                                                                         GskRenderPassType               pass_type,
                                                                         cairo_region_t                 *clip,
                                                                         const graphene_rect_t          *viewport,
                                                                         GskRenderNode                  *node);

GskGpuFrame *           gsk_gpu_occlusion_get_frame                     (GskGpuOcclusion                *self);

gboolean                gsk_gpu_occlusion_push_transform                (GskGpuOcclusion                *self,
                                                                         GskTransform                   *transform,
                                                                         GskGpuTransform                *out_save);
void                    gsk_gpu_occlusion_pop_transform                 (GskGpuOcclusion                *self,
                                                                         const GskGpuTransform          *saved);
GskGpuRenderPass *      gsk_gpu_occlusion_begin_rendering_whatever      (GskGpuOcclusion                *self);
GskGpuRenderPass *      gsk_gpu_occlusion_begin_rendering_transparent   (GskGpuOcclusion                *self);
GskGpuRenderPass *      gsk_gpu_occlusion_begin_rendering_color         (GskGpuOcclusion                *self,
                                                                         const GdkColor                 *color);
GskGpuRenderPass *      gsk_gpu_occlusion_try_node                      (GskGpuOcclusion                *self,
                                                                         GskRenderNode                  *node,
                                                                         gsize                           pos);
GskGpuRenderPass *      gsk_gpu_occlusion_try_node_untracked            (GskGpuOcclusion                *self,
                                                                         GskRenderNode                  *node);

/*** REMOVE ONCE REFACTORING DONE ***/

GskGpuRenderPass *      gsk_render_node_default_occlusion               (GskRenderNode                 *self,
                                                                         GskGpuOcclusion               *occlusion);
GskGpuRenderPass *      gsk_container_node_occlusion                    (GskRenderNode                 *node,
                                                                         GskGpuOcclusion               *occlusion);

G_END_DECLS


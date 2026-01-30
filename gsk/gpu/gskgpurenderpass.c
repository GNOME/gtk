#include "config.h"

#include "gskgpurenderpassprivate.h"

#include "gskgpuimageprivate.h"

#include "gsktransform.h"

void
gsk_gpu_render_pass_init (GskGpuRenderPass            *self,
                          GskGpuFrame                 *frame,
                          GskGpuImage                 *target,
                          GdkColorState               *ccs,
                          GskRenderPassType            pass_type,
                          const cairo_rectangle_int_t *clip,
                          const graphene_rect_t       *viewport)
{
  gsize width, height;

  width = gsk_gpu_image_get_width (target);
  height = gsk_gpu_image_get_height (target);

  self->frame = frame;
  self->pass_type = pass_type;
  self->ccs = ccs;

  self->scissor = *clip;
  self->blend = GSK_GPU_BLEND_OVER;
  if (clip->x == 0 && clip->y == 0 && clip->width == width && clip->height == height)
    {
      gsk_gpu_clip_init_empty (&self->clip, &GRAPHENE_RECT_INIT (0, 0, viewport->size.width, viewport->size.height));
    }
  else
    {
      float scale_x = viewport->size.width / width;
      float scale_y = viewport->size.height / height;
      gsk_gpu_clip_init_empty (&self->clip,
                               &GRAPHENE_RECT_INIT (
                                   scale_x * clip->x,
                                   scale_y * clip->y,
                                   scale_x * clip->width,
                                   scale_y * clip->height
                               ));
    }

  self->modelview = NULL;
  gsk_gpu_image_get_projection_matrix (target, &self->projection);
  graphene_vec2_init (&self->scale,
                      width / viewport->size.width,
                      height / viewport->size.height);
  self->offset = GRAPHENE_POINT_INIT (-viewport->origin.x,
                                      -viewport->origin.y);
  self->opacity = 1.0;
  self->pending_globals = GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND;
}

void
gsk_gpu_render_pass_finish (GskGpuRenderPass *self)
{
  g_clear_pointer (&self->modelview, gsk_transform_unref);
}


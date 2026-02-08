#include "config.h"

#include "gskgpurenderpassprivate.h"

#include "gskgpuimageprivate.h"
#include "gskgpurenderpassopprivate.h"

#include "gskrectprivate.h"
#include "gsktransform.h"

void
gsk_gpu_render_pass_init (GskGpuRenderPass            *self,
                          GskGpuFrame                 *frame,
                          GskGpuImage                 *target,
                          GdkColorState               *ccs,
                          GskRenderPassType            pass_type,
                          GskGpuLoadOp                 load_op,
                          float                        clear_color[4],
                          const cairo_rectangle_int_t *clip,
                          const graphene_rect_t       *viewport)
{
  gsize width, height;

  width = gsk_gpu_image_get_width (target);
  height = gsk_gpu_image_get_height (target);

  self->frame = frame;
  self->target = target;
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

  gsk_gpu_render_pass_begin_op (frame,
                                target,
                                clip,
                                load_op,
                                clear_color,
                                pass_type);
}

void
gsk_gpu_render_pass_finish (GskGpuRenderPass *self)
{
  gsk_gpu_render_pass_end_op (self->frame,
                              self->target,
                              self->pass_type);

  g_clear_pointer (&self->modelview, gsk_transform_unref);
}

void
gsk_gpu_render_pass_set_transform (GskGpuRenderPass *self,
                                   GskGpuTransform  *transform)
{
  gsk_gpu_clip_init_empty (&self->clip, &GSK_RECT_INIT_CAIRO (&self->scissor));
  gsk_gpu_clip_scale (&self->clip,
                      &self->clip,
                      transform->dihedral,
                      transform->scale.width,
                      transform->scale.height);
  self->offset = transform->offset;
  graphene_vec2_init (&self->scale, transform->scale.width, transform->scale.height);
  self->pending_globals |= GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;
  if (self->modelview || transform->dihedral != GDK_DIHEDRAL_NORMAL)
    {
      g_clear_pointer (&self->modelview, gsk_transform_unref);
      self->pending_globals |= GSK_GPU_GLOBAL_MATRIX;
      self->modelview = gsk_transform_dihedral (NULL, transform->dihedral);
    }
}

void
gsk_gpu_render_pass_push_translate (GskGpuRenderPass                 *self,
                                    const graphene_point_t           *offset,
                                    GskGpuRenderPassTranslateStorage *storage)
{
  storage->offset = self->offset;
  self->offset.x += offset->x;
  self->offset.y += offset->y;
}

void
gsk_gpu_render_pass_pop_translate (GskGpuRenderPass                 *self,
                                   GskGpuRenderPassTranslateStorage *storage)
{
  self->offset = storage->offset;
}

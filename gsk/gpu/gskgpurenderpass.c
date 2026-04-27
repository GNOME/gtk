#include "config.h"

#include "gskgpurenderpassprivate.h"

#include "gskglimageprivate.h"
#include "gskgpublendopprivate.h"
#include "gskgpuborderopprivate.h"
#include "gskgpucoloropprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuglobalsopprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpurenderpassopprivate.h"
#include "gskgpuroundedcoloropprivate.h"
#include "gskgpuscissoropprivate.h"
#include "gskgputextureopprivate.h"

#include "gskdebugprivate.h"
#include "gskrectprivate.h"
#include "gsktransform.h"

#include "gdk/gdkcolorprivate.h"

/* the epsilon we allow pixels to be off due to rounding errors.
 * Chosen rather randomly.
 */
#define EPSILON 0.001

/* the minimum scale required. If it's smaller than this, we treat it like 0 and
 * skip it.
 *
 * We set this to 1/100,000 because then 100,000 units would make up 1 pixel. And
 * it's unlikely that something useful results from scaling this far, considering
 * max texture size is usually 16,384 and FLOAT16 can't represent numbers this big.
 */
#define GSK_GPU_MIN_SCALE 0.00001

gboolean
gsk_gpu_render_pass_device_to_user (GskGpuRenderPass            *self,
                                    const cairo_rectangle_int_t *device,
                                    graphene_rect_t             *user)
{
  GskTransform *inverse;
  graphene_rect_t tmp;

  if (gsk_transform_get_fine_category (self->modelview) < GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL)
    return FALSE;

  inverse = gsk_transform_invert (gsk_transform_ref (self->modelview));
  gsk_transform_transform_bounds (inverse, &GSK_RECT_INIT_CAIRO (device), &tmp);
  gsk_transform_unref (inverse);

  *user = GRAPHENE_RECT_INIT (tmp.origin.x / self->scale.width - self->offset.x,
                              tmp.origin.y / self->scale.height - self->offset.y,
                              tmp.size.width / self->scale.width,
                              tmp.size.height / self->scale.height);

  return TRUE;
}

static gboolean
gsk_gpu_render_pass_user_to_device (GskGpuRenderPass      *self,
                                    const graphene_rect_t *user,
                                    graphene_rect_t       *device)
{
  graphene_rect_t tmp;

  if (gsk_transform_get_fine_category (self->modelview) < GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL)
    return FALSE;

  tmp = GRAPHENE_RECT_INIT ((user->origin.x + self->offset.x) * self->scale.width,
                            (user->origin.y + self->offset.y) * self->scale.height,
                            user->size.width * self->scale.width,
                            user->size.height * self->scale.height);

  gsk_transform_transform_bounds (self->modelview, &tmp, device);

  return TRUE;
}

gboolean
gsk_gpu_render_pass_user_to_device_shrink (GskGpuRenderPass      *self,
                                           const graphene_rect_t *user,
                                           cairo_rectangle_int_t *device)
{
  graphene_rect_t tmp;

  if (!gsk_gpu_render_pass_user_to_device (self, user, &tmp))
    return FALSE;

  if (!gsk_rect_to_cairo_shrink (&tmp, device))
    return FALSE;

  return TRUE;
}

gboolean
gsk_gpu_render_pass_user_to_device_exact (GskGpuRenderPass      *self,
                                          const graphene_rect_t *user,
                                          cairo_rectangle_int_t *device)
{
  graphene_rect_t tmp;

  if (!gsk_gpu_render_pass_user_to_device (self, user, &tmp))
    return FALSE;

  if (!gsk_rect_to_cairo_exact (&tmp, device))
    return FALSE;

  return TRUE;
}

static void
gsk_gpu_render_pass_compute_mvp (GskGpuRenderPass  *self,
                                 graphene_matrix_t *out_mvp)
{
  if (self->modelview)
    {
      gsk_transform_to_matrix (self->modelview, out_mvp);
      graphene_matrix_multiply (out_mvp, &self->projection, out_mvp);
    }
  else
    graphene_matrix_init_from_matrix (out_mvp, &self->projection);
}

static void
gsk_gpu_render_pass_user_to_device_bounds (GskGpuRenderPass      *self,
                                           const graphene_rect_t *user,
                                           graphene_rect_t       *device)
{
  graphene_rect_t tmp;
  graphene_matrix_t mvp;
  float scale_x, scale_y;

  /* 1. transform to ccordinate space of MVP */
  tmp = GRAPHENE_RECT_INIT ((user->origin.x + self->offset.x) * self->scale.width,
                            (user->origin.y + self->offset.y) * self->scale.height,
                             user->size.width * self->scale.width,
                             user->size.height * self->scale.height);

  /* 2. apply MVP to move to normalized device coordinates (NDC) and project
   *    back into 2D space */
  gsk_gpu_render_pass_compute_mvp (self, &mvp);
  graphene_matrix_transform_bounds (&mvp, &tmp, &tmp);

  /* 3. go back from NDC into device space
   *    This is a little hack that's faster than multiplying by the matrix) */
  scale_x = graphene_matrix_get_x_scale (&self->projection);
  scale_y = graphene_matrix_get_y_scale (&self->projection);
  *device = GRAPHENE_RECT_INIT ((tmp.origin.x - graphene_matrix_get_x_translation (&self->projection)) / scale_x,
                                (tmp.origin.y - graphene_matrix_get_y_translation (&self->projection)) / scale_y,
                                tmp.size.width / scale_x,
                                tmp.size.height / scale_y);
  gsk_rect_normalize (device);
}

GskGpuRenderPass *
gsk_gpu_render_pass_new (GskGpuFrame                 *frame,
                         GskGpuImage                 *target,
                         GdkColorState               *ccs,
                         GskRenderPassType            pass_type,
                         GskGpuLoadOp                 load_op,
                         float                        clear_color[4],
                         const cairo_rectangle_int_t *clip,
                         const graphene_rect_t       *viewport)
{
  GskGpuRenderPass *self;
  gsize width, height;

  self = g_new0 (GskGpuRenderPass, 1);

  width = gsk_gpu_image_get_width (target);
  height = gsk_gpu_image_get_height (target);

  self->frame = frame;
  self->target = target;
  self->pass_type = pass_type;
  self->ccs = ccs;

  self->scissor = *clip;
  self->blend = GSK_GPU_BLEND_OVER;
  self->offset = GRAPHENE_POINT_INIT (-viewport->origin.x,
                                      -viewport->origin.y);
  if (clip->x == 0 && clip->y == 0 && clip->width == width && clip->height == height)
    {
      gsk_gpu_clip_init_empty (&self->clip, &self->offset, viewport);
    }
  else
    {
      float scale_x = viewport->size.width / width;
      float scale_y = viewport->size.height / height;
      gsk_gpu_clip_init_empty (&self->clip,
                               graphene_point_zero (),
                               &GRAPHENE_RECT_INIT (
                                   scale_x * clip->x,
                                   scale_y * clip->y,
                                   scale_x * clip->width,
                                   scale_y * clip->height
                               ));
    }

  self->clip_mask = NULL;
  self->clip_mask_rect = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  self->clip_mask_has_opacity = FALSE;
  self->modelview = NULL;
  gsk_gpu_image_get_projection_matrix (target, &self->projection);
  self->scale = GRAPHENE_SIZE_INIT (width / viewport->size.width,
                                    height / viewport->size.height);
  self->opacity = 1.0;
  self->pending_globals = GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND;

  gsk_gpu_render_pass_begin_op (frame,
                                target,
                                clip,
                                load_op,
                                clear_color,
                                pass_type);

  return self;
}

void
gsk_gpu_render_pass_free (GskGpuRenderPass *self)
{
  gsk_gpu_render_pass_end_op (self->frame,
                              self->target,
                              self->pass_type);

  g_clear_pointer (&self->modelview, gsk_transform_unref);

  g_free (self);
}

void
gsk_gpu_render_pass_prepare_shader (GskGpuRenderPass *self)
{
  if (self->pending_globals == 0)
    return;

  if (self->pending_globals & (GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_MASK))
    {
      graphene_matrix_t mvp;

      gsk_gpu_render_pass_compute_mvp (self, &mvp);

      /* gl_FragCoord is origin bottom left and no way to change in GLES,
       * so we have to mess up the variable */
      if (GSK_IS_GL_IMAGE (self->target) &&
          gsk_gl_image_is_flipped (GSK_GL_IMAGE (self->target)) &&
          self->clip_mask_rect.size.height > 0)
        {
          float size = gsk_gpu_image_get_height (self->target);
          gsk_gpu_globals_op (self->frame,
                              &self->scale,
                              &mvp,
                              &GRAPHENE_RECT_INIT (self->clip_mask_rect.origin.x,
                                                   size - self->clip_mask_rect.origin.y,
                                                   self->clip_mask_rect.size.width,
                                                   - self->clip_mask_rect.size.height),
                              &self->clip.rect);
        }
      else
        {
          gsk_gpu_globals_op (self->frame,
                              &self->scale,
                              &mvp,
                              &self->clip_mask_rect,
                              &self->clip.rect);
        }
    }

  if (self->pending_globals & GSK_GPU_GLOBAL_SCISSOR)
    {
      gsk_gpu_scissor_op (self->frame,
                          &self->scissor);
    }

  if (self->pending_globals & GSK_GPU_GLOBAL_BLEND)
    {
      gsk_gpu_blend_op (self->frame, self->blend);
    }

  self->pending_globals = 0;
}

void
gsk_gpu_render_pass_push_blend (GskGpuRenderPass             *self,
                                GskGpuBlend                   blend,
                                GskGpuRenderPassBlendStorage *storage)
{
  storage->blend = self->blend;
  
  self->blend = blend;

  if (storage->blend != self->blend)
    self->pending_globals |= GSK_GPU_GLOBAL_BLEND;
}

void
gsk_gpu_render_pass_pop_blend (GskGpuRenderPass             *self,
                               GskGpuRenderPassBlendStorage *storage)
{
  if (storage->blend != self->blend)
    self->pending_globals |= GSK_GPU_GLOBAL_BLEND;

  self->blend = storage->blend;
}

void
gsk_gpu_render_pass_push_opacity (GskGpuRenderPass               *self,
                                  float                           opacity,
                                  GskGpuRenderPassOpacityStorage *storage)
{
  storage->opacity = self->opacity;
  self->opacity *= opacity;
}

void
gsk_gpu_render_pass_pop_opacity (GskGpuRenderPass               *self,
                                 GskGpuRenderPassOpacityStorage *storage)
{
  self->opacity = storage->opacity;
}

gboolean
gsk_gpu_render_pass_has_opacity (GskGpuRenderPass *self)
{
  return self->opacity < 1.0 || self->clip_mask_has_opacity;
}

void
gsk_gpu_render_pass_set_transform (GskGpuRenderPass *self,
                                   GskGpuTransform  *transform)
{
  gsk_gpu_clip_init_empty (&self->clip, graphene_point_zero (), &GSK_RECT_INIT_CAIRO (&self->scissor));
  gsk_gpu_clip_scale (&self->clip,
                      &self->clip,
                      transform->dihedral,
                      transform->scale.width,
                      transform->scale.height);
  self->offset = transform->offset;
  self->scale = GRAPHENE_SIZE_INIT (transform->scale.width, transform->scale.height);
  self->pending_globals |= GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;
  if (self->modelview || transform->dihedral != GDK_DIHEDRAL_NORMAL)
    {
      g_clear_pointer (&self->modelview, gsk_transform_unref);
      self->pending_globals |= GSK_GPU_GLOBAL_MATRIX;
      self->modelview = gsk_transform_dihedral (NULL, transform->dihedral);
    }
}

static void
extract_scale_from_transform (GskTransform *transform,
                              float        *out_scale_x,
                              float        *out_scale_y)
{
  switch (gsk_transform_get_fine_category (transform))
    {
    default:
      g_assert_not_reached ();
    case GSK_FINE_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE:
      *out_scale_x = 1.0f;
      *out_scale_y = 1.0f;
      return;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float scale_x, scale_y, dx, dy;
        gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
        *out_scale_x = scale_x;
        *out_scale_y = scale_y;
      }
      return;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL:
    case GSK_FINE_TRANSFORM_CATEGORY_2D:
      {
        float skew_x, skew_y, scale_x, scale_y, angle, dx, dy;
        gsk_transform_to_2d_components (transform,
                                        &skew_x, &skew_y,
                                        &scale_x, &scale_y,
                                        &angle,
                                        &dx, &dy);
        *out_scale_x = fabs (scale_x);
        *out_scale_y = fabs (scale_y);
      }
      return;

    case GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_FINE_TRANSFORM_CATEGORY_ANY:
    case GSK_FINE_TRANSFORM_CATEGORY_3D:
      {
        graphene_quaternion_t rotation;
        graphene_matrix_t matrix;
        graphene_vec4_t perspective;
        graphene_vec3_t translation;
        graphene_vec3_t matrix_scale;
        graphene_vec3_t shear;

        gsk_transform_to_matrix (transform, &matrix);
        if (!graphene_matrix_decompose (&matrix,
                                        &translation,
                                        &matrix_scale,
                                        &rotation,
                                        &shear,
                                        &perspective))
          {
            *out_scale_x = 0;
            *out_scale_y = 0;
            return;
          }

        *out_scale_x = fabs (graphene_vec3_get_x (&matrix_scale));
        *out_scale_y = fabs (graphene_vec3_get_y (&matrix_scale));
      }
      return;
    }
}

static gboolean
gsk_gpu_render_pass_push_transform_check_scale (GskGpuRenderPass                 *self,
                                                float                             scale_x,
                                                float                             scale_y,
                                                GskGpuRenderPassTransformStorage *storage)
{
  if (scale_x >= GSK_GPU_MIN_SCALE &&
      scale_y >= GSK_GPU_MIN_SCALE)
    return TRUE;

  gsk_gpu_clip_init_all_clipped (&self->clip);
  self->modelview = gsk_transform_ref (storage->modelview);
  storage->clip.modified |= GSK_GPU_GLOBAL_CLIP;
  self->pending_globals |= storage->clip.modified;

  return FALSE;
}

static void
gsk_gpu_render_pass_draw_clip_mask (GskGpuRenderPass            *self,
                                    const graphene_rect_t       *new_clip_rect,
                                    const GskRoundedRect        *new_clip_rounded,
                                    GskGpuImage                 *new_clip_mask,
                                    const graphene_rect_t       *new_clip_mask_rect,
                                    GskGpuRenderPassClipStorage *storage);

void
gsk_gpu_render_pass_push_transform (GskGpuRenderPass                 *self,
                                    GskTransform                     *transform,
                                    const graphene_rect_t            *bounds,
                                    const graphene_rect_t            *child_bounds,
                                    GskGpuRenderPassTransformStorage *storage)
{
  storage->modelview = self->modelview;
  storage->scale = self->scale;
  storage->offset = self->offset;
  gsk_gpu_clip_init_copy (&storage->clip.clip, &self->clip);
  storage->clip.modified = 0;

  switch (gsk_transform_get_fine_category (transform))
    {
    case GSK_FINE_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE:
      /* We assume these 2 to be special-cased by the caller, so we just */
      G_GNUC_FALLTHROUGH;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float dx, dy, scale_x, scale_y;

        gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
        if (!gsk_gpu_render_pass_push_transform_check_scale (self,
                                                             scale_x * storage->scale.width,
                                                             scale_y * storage->scale.height,
                                                             storage))
          return;

        gsk_gpu_clip_scale (&self->clip, &storage->clip.clip, GDK_DIHEDRAL_NORMAL, scale_x, scale_y);
        self->offset.x = (self->offset.x + dx) / scale_x;
        self->offset.y = (self->offset.y + dy) / scale_y;
        self->scale = GRAPHENE_SIZE_INIT (scale_x * storage->scale.width,
                                          scale_y * storage->scale.height);
        self->modelview = gsk_transform_ref (storage->modelview);
        storage->clip.modified = GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;
      }
      break;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL:
      {
        GdkDihedral dihedral, inverted;
        float xx, xy, yx, yy, dx, dy, scale_x, scale_y, old_scale_x, old_scale_y;

        gsk_transform_to_dihedral (transform, &dihedral, &scale_x, &scale_y, &dx, &dy);
        inverted = gdk_dihedral_invert (dihedral);
        gdk_dihedral_get_mat2 (inverted, &xx, &xy, &yx, &yy);
        old_scale_x = storage->scale.width;
        old_scale_y = storage->scale.height;
        if (!gsk_gpu_render_pass_push_transform_check_scale (self,
                                                             fabs (scale_x * (old_scale_x * xx + old_scale_y * yx)),
                                                             fabs (scale_y * (old_scale_x * xy + old_scale_y * yy)),
                                                             storage))
          return;
        gsk_gpu_clip_scale (&self->clip, &storage->clip.clip, inverted, scale_x, scale_y);
        self->offset.x = (self->offset.x + dx) / scale_x;
        self->offset.y = (self->offset.y + dy) / scale_y;
        self->offset = GRAPHENE_POINT_INIT (xx * self->offset.x + xy * self->offset.y,
                                            yx * self->offset.x + yy * self->offset.y);
        self->scale = GRAPHENE_SIZE_INIT (fabs (scale_x * (old_scale_x * xx + old_scale_y * yx)),
                                          fabs (scale_y * (old_scale_x * xy + old_scale_y * yy)));
        self->modelview = gsk_transform_dihedral (gsk_transform_ref (storage->modelview), dihedral);
        storage->clip.modified = GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_MATRIX;
      }
      break;

    case GSK_FINE_TRANSFORM_CATEGORY_2D:
    case GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_FINE_TRANSFORM_CATEGORY_ANY:
    case GSK_FINE_TRANSFORM_CATEGORY_3D:
      {
        GskTransform *clip_transform;

        clip_transform = gsk_transform_transform (gsk_transform_translate (NULL, &self->offset), transform);

        if (child_bounds && gsk_gpu_clip_contains_rect (&self->clip, &self->offset, bounds))
          {
            gsk_gpu_clip_init_contained (&self->clip, child_bounds);
          }
        else 
          {
            GskTransform *inverse;
            graphene_rect_t new_bounds;

            if (storage->clip.clip.type != GSK_GPU_CLIP_NONE)
              {
                gsk_gpu_render_pass_draw_clip_mask (self, NULL, NULL, NULL, NULL, &storage->clip);
              }
            if (!gsk_gpu_clip_is_all_clipped (&self->clip))
              {
                /* after drawing a clip mask, the clip is either empty or everything is clipped */
                g_assert (self->clip.type == GSK_GPU_CLIP_NONE);
                inverse = gsk_transform_invert (gsk_transform_ref (clip_transform));
                gsk_transform_transform_bounds (inverse, &storage->clip.clip.rect.bounds, &new_bounds);
                gsk_transform_unref (inverse);
                gsk_gpu_clip_init_empty (&self->clip, graphene_point_zero (), &new_bounds);
              }
          }

        self->modelview = gsk_transform_scale (gsk_transform_ref (storage->modelview),
                                               self->scale.width,
                                               self->scale.height);
        self->modelview = gsk_transform_translate (self->modelview, &self->offset);
        self->modelview = gsk_transform_transform (self->modelview, transform);

        if (gsk_transform_get_fine_category (self->modelview) >= GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL)
          {
            float scale_x, scale_y, dx, dy, xx, xy, yx, yy;
            graphene_rect_t scissor_rect;
            GdkDihedral dihedral, inverted;
            GskGpuClip scissored_clip;

            /* we magically transformed back into a simple transform */
            gsk_transform_to_dihedral (self->modelview, &dihedral, &scale_x, &scale_y, &dx, &dy);
            inverted = gdk_dihedral_invert (dihedral);
            gdk_dihedral_get_mat2 (inverted, &xx, &xy, &yx, &yy);
            if (!gsk_gpu_render_pass_push_transform_check_scale (self,
                                                                 fabs (scale_x * xx + scale_y * yx),
                                                                 fabs (scale_x * xy + scale_y * yy),
                                                                 storage))
              return;
            dx /= scale_x;
            dy /= scale_y;
            self->offset = GRAPHENE_POINT_INIT (xx * dx + xy * dy,
                                                yx * dx + yy * dy);
            self->scale = GRAPHENE_SIZE_INIT (fabs (scale_x * xx + scale_y * yx),
                                              fabs (scale_x * xy + scale_y * yy));
            g_clear_pointer (&self->modelview, gsk_transform_unref);
            self->modelview = gsk_transform_dihedral (NULL, dihedral);
            if (!gsk_gpu_clip_is_all_clipped (&self->clip))
              {
                self->clip.rect.bounds.origin.x += self->offset.x;
                self->clip.rect.bounds.origin.y += self->offset.y;

                /* We can now check the clip against the scissor rect again */
                if (!gsk_gpu_render_pass_device_to_user (self, &self->scissor, &scissor_rect))
                  {
                    /* Only happens with more complex transforms */
                    g_assert_not_reached ();
                  }
                if (gsk_gpu_clip_intersect_rect (&scissored_clip, &self->clip, &self->offset, &scissor_rect))
                  gsk_gpu_clip_init_copy (&self->clip, &scissored_clip);
              }
          }
        else
          {
            float scale_x, scale_y, old_pixels, new_pixels;

            extract_scale_from_transform (self->modelview, &scale_x, &scale_y);

            old_pixels = MAX (storage->scale.width * storage->clip.clip.rect.bounds.size.width,
                              storage->scale.height * storage->clip.clip.rect.bounds.size.height);
            new_pixels = MAX (scale_x * self->clip.rect.bounds.size.width,
                              scale_y * self->clip.rect.bounds.size.height);
            /* Check that our offscreen doesn't get too big.  1.5 ~ sqrt(2) */
            if (new_pixels > 1.5 * old_pixels)
              {
                float forced_downscale = 2 * old_pixels / new_pixels;
                scale_x *= forced_downscale;
                scale_y *= forced_downscale;
              }

            if (!gsk_gpu_render_pass_push_transform_check_scale (self,
                                                                 scale_x,
                                                                 scale_y,
                                                                 storage))
              return;

            self->modelview = gsk_transform_scale (self->modelview, 1 / scale_x, 1 / scale_y);
            self->scale = GRAPHENE_SIZE_INIT (scale_x, scale_y);
            self->offset = GRAPHENE_POINT_INIT (0, 0);
          }
        storage->clip.modified |= GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_MATRIX;
      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  self->pending_globals |= storage->clip.modified;
}

void
gsk_gpu_render_pass_pop_transform (GskGpuRenderPass                 *self,
                                   GskGpuRenderPassTransformStorage *storage)
{
  gsk_gpu_render_pass_pop_clip_rect (self, &storage->clip);
  g_clear_pointer (&self->modelview, gsk_transform_unref);
  self->modelview = storage->modelview;
  self->scale = storage->scale;
  self->offset = storage->offset;
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

gboolean
gsk_gpu_render_pass_is_all_clipped (GskGpuRenderPass *self)
{
  return gsk_gpu_clip_is_all_clipped (&self->clip);
}

gboolean
gsk_gpu_render_pass_get_clip_bounds (GskGpuRenderPass *self,
                                     graphene_rect_t  *out_bounds)
{
  graphene_rect_t scissor;

  if (gsk_gpu_clip_is_all_clipped (&self->clip))
    return FALSE;

  if (gsk_gpu_render_pass_device_to_user (self,
                                          &self->scissor,
                                          &scissor))
    {
      graphene_rect_t tmp;

      gsk_rect_init_offset (&tmp,
                            &self->clip.rect.bounds,
                            &GRAPHENE_POINT_INIT (-self->offset.x, -self->offset.y));

      if (!gsk_rect_intersection (&scissor, &tmp, out_bounds))
        {
          g_warning ("Clipping is broken, everything is clipped, but we didn't set all-clipped.");
          return FALSE;
        }

      return TRUE;
    }
  else
    {
      gsk_rect_init_offset (out_bounds,
                            &self->clip.rect.bounds,
                            &GRAPHENE_POINT_INIT (-self->offset.x, -self->offset.y));
      return TRUE;
    }
}

void
gsk_gpu_render_pass_push_clip_device_rect (GskGpuRenderPass            *self,
                                           const cairo_rectangle_int_t *clip,
                                           GskGpuRenderPassClipStorage *storage)
{
  graphene_rect_t scissor;

  gsk_gpu_clip_init_copy (&storage->clip, &self->clip);
  storage->scissor = self->scissor;

  if (!gdk_rectangle_intersect (&self->scissor, clip, &self->scissor))
    {
      gsk_gpu_clip_init_copy (&storage->clip, &self->clip);
      gsk_gpu_clip_init_all_clipped (&self->clip);
      storage->modified = GSK_GPU_GLOBAL_CLIP;
      self->pending_globals |= storage->modified;
      return;
    }

  storage->modified = GSK_GPU_GLOBAL_SCISSOR;
  if (gsk_gpu_render_pass_device_to_user (self, &self->scissor, &scissor) &&
      gsk_gpu_clip_intersect_rect (&self->clip, &storage->clip, &self->offset, &scissor))
    {
      /* if scissoring does all the work, we can pretend the clip is empty */
      if (gsk_gpu_clip_contains_rect (&self->clip, &self->offset, &scissor))
        {
          gsk_gpu_clip_init_empty (&self->clip, &self->offset, &scissor);
        }
      storage->modified |= GSK_GPU_GLOBAL_CLIP;
    }
  else
    {
      gsk_gpu_clip_init_copy (&self->clip, &storage->clip);
    }
}

void
gsk_gpu_render_pass_pop_clip_device_rect (GskGpuRenderPass            *self,
                                          GskGpuRenderPassClipStorage *storage)
{
  /* They're identical currently */
  gsk_gpu_render_pass_pop_clip_rect (self, storage);
}

static gboolean
gsk_gpu_render_pass_try_push_clip_rect (GskGpuRenderPass            *self,
                                        const graphene_rect_t       *clip,
                                        GskGpuRenderPassClipStorage *storage)
{
  cairo_rectangle_int_t scissor;
  graphene_rect_t scissored;

  if (gsk_gpu_clip_is_all_clipped (&self->clip))
    {
      storage->modified = 0;
      return TRUE;
    }

  if (gsk_gpu_render_pass_device_to_user (self,
                                          &self->scissor,
                                          &scissored))
    {
      if (!gsk_rect_intersection (&scissored, clip, &scissored))
        {
          gsk_gpu_clip_init_copy (&storage->clip, &self->clip);
          gsk_gpu_clip_init_all_clipped (&self->clip);
          storage->modified = GSK_GPU_GLOBAL_CLIP;
          self->pending_globals |= storage->modified;
          return TRUE;
        }
    }
  else
    scissored = *clip;

  /* Check if we can use scissoring for the clip */
  /* We could check scissoring for each edge individually, but that's a lot
   * more code */
  if (gsk_gpu_render_pass_user_to_device_exact (self, &scissored, &scissor))
    {
      storage->scissor = self->scissor;
      storage->modified = GSK_GPU_GLOBAL_SCISSOR;
      self->scissor = scissor;

      gsk_gpu_clip_init_copy (&storage->clip, &self->clip);

      if (gsk_gpu_clip_intersect_rect (&self->clip, &storage->clip, &self->offset, &scissored))
        {
          /* if scissoring does all the work, we can pretend the clip is empty */
          if (gsk_gpu_clip_contains_rect (&self->clip, &self->offset, &scissored))
            {
              gsk_gpu_clip_init_empty (&self->clip, &self->offset, &scissored);
            }
          storage->modified |= GSK_GPU_GLOBAL_CLIP;
        }
      else
        {
          gsk_gpu_clip_init_copy (&self->clip, &storage->clip);
        }
    }
  else
    {
      gsk_gpu_clip_init_copy (&storage->clip, &self->clip);

      if (gsk_gpu_clip_intersect_rect (&self->clip, &storage->clip, &self->offset, &scissored))
        {
          storage->modified = GSK_GPU_GLOBAL_CLIP;
        }
      else
        {
          gsk_gpu_clip_init_copy (&self->clip, &storage->clip);
          return FALSE;
        }

    }

  self->pending_globals |= storage->modified;
  return TRUE;
}

static void
gsk_gpu_render_pass_clear_all (GskGpuRenderPass      *self,
                               const graphene_rect_t *rect)
{
  GdkColor white;
  GskGpuRenderPassBlendStorage storage;

  gdk_color_init (&white, self->ccs, (float[4]) { 1, 1, 1, 1 });
  gsk_gpu_render_pass_push_blend (self, GSK_GPU_BLEND_CLEAR, &storage);

  gsk_gpu_color_op (self,
                    self->ccs,
                    self->ccs,
                    rect,
                    &white);

  gsk_gpu_render_pass_pop_blend (self, &storage);
  gdk_color_finish (&white);
}

static void
gsk_gpu_render_pass_clear_rect (GskGpuRenderPass      *self,
                                const graphene_rect_t *outer,
                                const graphene_rect_t *inner)
{
  graphene_vec4_t widths;
  float borders[4];
  GdkColor white;
  GskGpuRenderPassBlendStorage storage;

  borders[0] = MAX (inner->origin.y - outer->origin.y, 0);
  borders[1] = MAX (outer->origin.x + outer->size.width - inner->origin.x - inner->size.width, 0);
  borders[2] = MAX (outer->origin.y + outer->size.height - inner->origin.y - inner->size.height, 0);
  borders[3] = MAX (inner->origin.x - outer->origin.x, 0);
  if (borders[0] <= 0 && borders[1] <= 0 &&
      borders[2] <= 0 && borders[3] <= 0)
    return;

  gdk_color_init (&white, self->ccs, (float[4]) { 1, 1, 1, 1 });
  gsk_gpu_render_pass_push_blend (self, GSK_GPU_BLEND_CLEAR, &storage);

  graphene_vec4_init_from_float (&widths, borders);
  gsk_gpu_border_op (self,
                     self->ccs,
                     self->ccs,
                     outer,
                     &GSK_ROUNDED_RECT_INIT_FROM_RECT (*outer),
                     &white,
                     &white,
                     &white,
                     &white,
                     &widths,
                     &GRAPHENE_SIZE_INIT (0, 0));

  gsk_gpu_render_pass_pop_blend (self, &storage);
  gdk_color_finish (&white);
}

static void
gsk_gpu_render_pass_clear_rounded (GskGpuRenderPass      *self,
                                   const graphene_rect_t *outer,
                                   const GskRoundedRect  *inner)
{
  graphene_vec4_t widths;
  float borders[4];
  GskRoundedRect r;
  GdkColor white;
  GskGpuRenderPassBlendStorage storage;

  r = *inner;

  borders[0] = MAX (r.bounds.origin.y - outer->origin.y, 0);
  borders[0] += MAX (r.corner[GSK_CORNER_TOP_LEFT].height,
                     r.corner[GSK_CORNER_TOP_RIGHT].height);
  borders[1] = MAX (outer->origin.x + outer->size.width - r.bounds.origin.x - r.bounds.size.width, 0);
  borders[1] += MAX (r.corner[GSK_CORNER_TOP_RIGHT].width,
                     r.corner[GSK_CORNER_BOTTOM_RIGHT].width);
  borders[2] = MAX (outer->origin.y + outer->size.height - r.bounds.origin.y - r.bounds.size.height, 0);
  borders[2] += MAX (r.corner[GSK_CORNER_BOTTOM_LEFT].height,
                     r.corner[GSK_CORNER_BOTTOM_RIGHT].height);
  borders[3] = MAX (r.bounds.origin.x - outer->origin.x, 0);
  borders[3] += MAX (r.corner[GSK_CORNER_TOP_LEFT].width,
                     r.corner[GSK_CORNER_BOTTOM_LEFT].width);
  if (borders[0] <= 0 && borders[1] <= 0 &&
      borders[2] <= 0 && borders[3] <= 0)
    return;

  gdk_color_init (&white, self->ccs, (float[4]) { 1, 1, 1, 1 });
  gsk_gpu_render_pass_push_blend (self, GSK_GPU_BLEND_CLEAR, &storage);

  graphene_vec4_init_from_float (&widths, borders);
  gsk_rounded_rect_shrink (&r, -borders[0], -borders[1], -borders[2], -borders[3]);
  gsk_gpu_border_op (self,
                     self->ccs,
                     self->ccs,
                     outer,
                     &r,
                     &white,
                     &white,
                     &white,
                     &white,
                     &widths,
                     &GRAPHENE_SIZE_INIT (0, 0));

  gsk_gpu_render_pass_pop_blend (self, &storage);
  gdk_color_finish (&white);
}

static void
gsk_gpu_render_pass_clear_clip (GskGpuRenderPass *self,
                                const GskGpuClip *clip)
{
  graphene_rect_t bounds;

  if (!gsk_gpu_render_pass_get_clip_bounds (self, &bounds))
    return;

  switch (clip->type)
    {
      case GSK_GPU_CLIP_ROUNDED:
        gsk_gpu_render_pass_clear_rounded (self,
                                           &bounds,
                                           &clip->rect);
        break;

      case GSK_GPU_CLIP_RECT:
      case GSK_GPU_CLIP_CONTAINED:
        gsk_gpu_render_pass_clear_rect (self, 
                                        &bounds,
                                        &clip->rect.bounds);
        break;

      case GSK_GPU_CLIP_NONE:
        break;

      case GSK_GPU_CLIP_ALL_CLIPPED:
        gsk_gpu_render_pass_clear_all (self, &bounds);
        break;

      default:
        g_assert_not_reached ();
        break;
    }
}

static void
gsk_gpu_render_pass_draw_clip_mask (GskGpuRenderPass            *self,
                                    const graphene_rect_t       *new_clip_rect,
                                    const GskRoundedRect        *new_clip_rounded,
                                    GskGpuImage                 *new_clip_mask,
                                    const graphene_rect_t       *new_clip_mask_rect,
                                    GskGpuRenderPassClipStorage *storage)
{
  GskGpuRenderPass *other;
  graphene_rect_t bounds, device;
  cairo_rectangle_int_t area;
  GskGpuImage *image;
  GskDebugProfile *profile;
  GdkMemoryFormat format;
  GskGpuRenderPassBlendStorage blend_storage;
  GskGpuRenderPassTransformStorage transform_storage;
  GskGpuRenderPassTranslateStorage translate_storage;
  GskTransform *transform;
  GskGpuClip old_clip;

  if (!gsk_gpu_render_pass_get_clip_bounds (self, &bounds) ||
      (new_clip_rect && !gsk_rect_intersection (&bounds, new_clip_rect, &bounds)) ||
      (new_clip_rounded && !gsk_rect_intersection (&bounds, &new_clip_rounded->bounds, &bounds)) ||
      (new_clip_mask_rect && !gsk_rect_intersection (&bounds, new_clip_mask_rect, &bounds)))
    {
      gsk_gpu_clip_init_copy (&storage->clip, &self->clip);
      gsk_gpu_clip_init_all_clipped (&self->clip);
      storage->modified = GSK_GPU_GLOBAL_CLIP;
      self->pending_globals |= storage->modified;
      return;
    }

  gsk_gpu_render_pass_user_to_device_bounds (self, &bounds, &device);

  gsk_rect_to_cairo_grow (&device, &area);
  if (!gdk_rectangle_intersect (&area,
                                &self->scissor,
                                &area))
    {
      gsk_gpu_clip_init_copy (&storage->clip, &self->clip);
      gsk_gpu_clip_init_all_clipped (&self->clip);
      storage->modified = GSK_GPU_GLOBAL_CLIP;
      self->pending_globals |= storage->modified;
      return;
    }

  format = gdk_memory_depth_get_alpha_format (gdk_memory_format_get_depth (gsk_gpu_image_get_format (self->target)));
  image = gsk_gpu_device_create_offscreen_image (gsk_gpu_frame_get_device (self->frame),
                                                 FALSE,
                                                 format,
                                                 FALSE,
                                                 area.width,
                                                 area.height);
  if (image == NULL)
    {
      gsk_gpu_clip_init_copy (&storage->clip, &self->clip);
      gsk_gpu_clip_init_all_clipped (&self->clip);
      storage->modified = GSK_GPU_GLOBAL_CLIP;
      self->pending_globals |= storage->modified;
      return;
    }

  profile = gsk_gpu_frame_get_profile (self->frame);
  if (profile)
    {
      profile->self.n_offscreens++;
      profile->self.offscreen_pixels += area.width * area.height;
    }

  other = gsk_gpu_render_pass_new (self->frame,
                                   image,
                                   self->ccs,
                                   GSK_RENDER_PASS_OFFSCREEN,
                                   GSK_GPU_LOAD_OP_CLEAR,
                                   (float[4]) { self->opacity, self->opacity, self->opacity, self->opacity },
                                   &(cairo_rectangle_int_t) {
                                     0, 0,
                                     area.width, area.height
                                   },
                                   &GSK_RECT_INIT_CAIRO (&area));

  gsk_gpu_render_pass_push_blend (other, GSK_GPU_BLEND_MASK, &blend_storage);

  if (self->clip_mask)
    {
      gsk_gpu_texture_op (other,
                          self->ccs,
                          &GSK_RECT_INIT_CAIRO (&area),
                          self->clip_mask,
                          GSK_GPU_SAMPLER_TRANSPARENT,
                          &self->clip_mask_rect);
    }

  transform = gsk_transform_ref (self->modelview);
  transform = gsk_transform_scale (transform,
                                   self->scale.width,
                                   self->scale.height);
  if (transform)
    gsk_gpu_render_pass_push_transform (other, transform, NULL, NULL, &transform_storage);

  gsk_gpu_render_pass_clear_clip (other, &self->clip);

  gsk_gpu_render_pass_push_translate (other, &self->offset, &translate_storage);

  if (!gsk_gpu_render_pass_get_clip_bounds (other, &bounds))
    {
      g_assert_not_reached ();
    }

  if (new_clip_mask)
    {
      /* we rely on the mask being transparent outside of the clip
       * rect here as bounds may be larger than new_clip_rect */
      gsk_gpu_texture_op (other,
                          self->ccs,
                          &bounds,
                          new_clip_mask,
                          GSK_GPU_SAMPLER_TRANSPARENT,
                          new_clip_mask_rect);
    }
  else if (new_clip_rect)
    {
      gsk_gpu_render_pass_clear_rect (other,
                                      &bounds,
                                      new_clip_rect);
    }

  if (new_clip_rounded)
    {
      gsk_gpu_render_pass_clear_rounded (other,
                                         &bounds,
                                         new_clip_rounded);
    }

  gsk_gpu_render_pass_pop_translate (other, &translate_storage);
  if (transform)
    {
      gsk_gpu_render_pass_pop_transform (other, &transform_storage);
      gsk_transform_unref (transform);
    }
  gsk_gpu_render_pass_pop_blend (other, &blend_storage);
  gsk_gpu_render_pass_free (other);

  /* We can reset things now, the mask does it all */
  gsk_gpu_clip_init_copy (&old_clip, &self->clip);
  gsk_gpu_clip_init_empty (&self->clip, &self->offset, &bounds);
  gsk_gpu_render_pass_push_clip_device_rect (self, &area, storage);
  gsk_gpu_clip_init_copy (&storage->clip, &old_clip);
  storage->clip_mask = self->clip_mask;
  storage->clip_mask_rect = self->clip_mask_rect;
  storage->clip_mask_has_opacity = self->clip_mask_has_opacity;
  storage->opacity = self->opacity;
  storage->modified |= GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_MASK;

  self->clip_mask = image;
  self->clip_mask_has_opacity |= self->opacity < 1.0;
  self->opacity = 1.0;
  self->clip_mask_rect = GSK_RECT_INIT_CAIRO (&area);
  /* We can reset things now, the mask does it all */
  gsk_gpu_clip_init_empty (&self->clip, &self->offset, &bounds);
  self->pending_globals |= storage->modified;
}

void
gsk_gpu_render_pass_push_clip_rect (GskGpuRenderPass            *self,
                                    const graphene_rect_t       *clip,
                                    GskGpuRenderPassClipStorage *storage)
{
  if (gsk_gpu_render_pass_try_push_clip_rect (self, clip, storage))
    return;

  GSK_DEBUG (FALLBACK, "push_clip_rect() needs clip mask");
  gsk_gpu_render_pass_draw_clip_mask (self, clip, NULL, NULL, NULL, storage);
}

void
gsk_gpu_render_pass_pop_clip_rect (GskGpuRenderPass            *self,
                                   GskGpuRenderPassClipStorage *storage)
{
  if (storage->modified & GSK_GPU_GLOBAL_SCISSOR)
    self->scissor = storage->scissor;

  if (storage->modified & GSK_GPU_GLOBAL_CLIP)
    gsk_gpu_clip_init_copy (&self->clip, &storage->clip);

  if (storage->modified & GSK_GPU_GLOBAL_MASK)
    {
      g_clear_object (&self->clip_mask);
      self->clip_mask = storage->clip_mask;
      self->clip_mask_rect = storage->clip_mask_rect;
      self->clip_mask_has_opacity = storage->clip_mask_has_opacity;
      self->opacity = storage->opacity;
    }

  self->pending_globals |= storage->modified;
}

void
gsk_gpu_render_pass_push_clip_rounded (GskGpuRenderPass            *self,
                                       const GskRoundedRect        *clip,
                                       GskGpuRenderPassClipStorage *storage)
{
  graphene_rect_t scissor;

  gsk_gpu_clip_init_copy (&storage->clip, &self->clip);
  storage->modified = GSK_GPU_GLOBAL_CLIP;

  if (!gsk_gpu_clip_intersect_rounded_rect (&self->clip, &storage->clip, &self->offset, clip))
    {
      gsk_gpu_clip_init_copy (&self->clip, &storage->clip);
      GSK_DEBUG (FALLBACK, "push_clip_rounded() needs clip mask");
      gsk_gpu_render_pass_draw_clip_mask (self, NULL, clip, NULL, NULL, storage);
      return;
    }

  if (gsk_gpu_render_pass_device_to_user (self,
                                          &self->scissor,
                                          &scissor))
    {
      GskGpuClip scissored_clip;
      if (gsk_gpu_clip_intersect_rect (&scissored_clip, &self->clip, &self->offset, &scissor))
        gsk_gpu_clip_init_copy (&self->clip, &scissored_clip);
    }

  self->pending_globals |= storage->modified;
}

void
gsk_gpu_render_pass_pop_clip_rounded (GskGpuRenderPass            *self,
                                      GskGpuRenderPassClipStorage *storage)
{
  /* They're identical currently */
  gsk_gpu_render_pass_pop_clip_rect (self, storage);
}

void
gsk_gpu_render_pass_push_clip_mask (GskGpuRenderPass            *self,
                                    const graphene_rect_t       *clip,
                                    GskGpuImage                 *clip_mask,
                                    const graphene_rect_t       *clip_mask_rect,
                                    gboolean                     has_opacity,
                                    GskGpuRenderPassClipStorage *storage)
{
  graphene_rect_t device;

  if (self->clip_mask != NULL ||
      gsk_transform_get_fine_category (self->modelview) <= GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE ||
      !gsk_gpu_render_pass_try_push_clip_rect (self, clip, storage) ||
      !gsk_gpu_render_pass_user_to_device (self, clip_mask_rect, &device))
    {
      GSK_DEBUG (FALLBACK, "push_clip_mask() needs to draw clip mask");
      gsk_gpu_render_pass_draw_clip_mask (self, clip, NULL, clip_mask, clip_mask_rect, storage);
      self->clip_mask_has_opacity |= has_opacity;
      return;
    }

  g_assert (gsk_gpu_image_get_shader_op (clip_mask) == GDK_SHADER_DEFAULT);

  storage->clip_mask = self->clip_mask;
  storage->clip_mask_rect = self->clip_mask_rect;
  storage->clip_mask_has_opacity = self->clip_mask_has_opacity;
  storage->opacity = self->opacity;
  storage->modified |= GSK_GPU_GLOBAL_MASK;

  self->clip_mask = g_object_ref (clip_mask);
  self->clip_mask_rect = device;
  self->clip_mask_has_opacity = has_opacity;

  self->pending_globals |= storage->modified;
}

void
gsk_gpu_render_pass_pop_clip_mask (GskGpuRenderPass            *self,
                                   GskGpuRenderPassClipStorage *storage)
{
  /* They're identical currently */
  gsk_gpu_render_pass_pop_clip_rect (self, storage);
}

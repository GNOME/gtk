#include "config.h"

#include "gskgpurenderpassprivate.h"

#include "gskgpuimageprivate.h"
#include "gskgpurenderpassopprivate.h"

#include "gskrectprivate.h"
#include "gsktransform.h"

static gboolean
gsk_gpu_render_pass_device_to_user (GskGpuRenderPass            *self,
                                    const cairo_rectangle_int_t *device,
                                    graphene_rect_t             *user)
{
  float scale_x, scale_y;
  GskTransform *inverse;
  graphene_rect_t tmp;

  if (gsk_transform_get_fine_category (self->modelview) < GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL)
    return FALSE;

  inverse = gsk_transform_invert (gsk_transform_ref (self->modelview));
  gsk_transform_transform_bounds (inverse, &GSK_RECT_INIT_CAIRO (device), &tmp);
  gsk_transform_unref (inverse);

  scale_x = graphene_vec2_get_x (&self->scale);
  scale_y = graphene_vec2_get_y (&self->scale);
  *user = GRAPHENE_RECT_INIT (tmp.origin.x / scale_x - self->offset.x,
                              tmp.origin.y / scale_y - self->offset.y,
                              tmp.size.width / scale_x,
                              tmp.size.height / scale_y);

  return TRUE;
}

static gboolean
gsk_gpu_render_pass_user_to_device (GskGpuRenderPass      *self,
                                    const graphene_rect_t *user,
                                    graphene_rect_t       *device)
{
  float scale_x, scale_y;
  graphene_rect_t tmp;

  if (gsk_transform_get_fine_category (self->modelview) < GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL)
    return FALSE;

  scale_x = graphene_vec2_get_x (&self->scale);
  scale_y = graphene_vec2_get_y (&self->scale);
  tmp = GRAPHENE_RECT_INIT ((user->origin.x + self->offset.x) * scale_x,
                            (user->origin.y + self->offset.y) * scale_y,
                            user->size.width * scale_x,
                            user->size.height * scale_y);

  gsk_transform_transform_bounds (self->modelview, &tmp, device);

  return TRUE;
}

static gboolean
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

  self->modelview = NULL;
  gsk_gpu_image_get_projection_matrix (target, &self->projection);
  graphene_vec2_init (&self->scale,
                      width / viewport->size.width,
                      height / viewport->size.height);
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
  graphene_vec2_init (&self->scale, transform->scale.width, transform->scale.height);
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
        graphene_matrix_decompose (&matrix,
                                   &translation,
                                   &matrix_scale,
                                   &rotation,
                                   &shear,
                                   &perspective);

        *out_scale_x = fabs (graphene_vec3_get_x (&matrix_scale));
        *out_scale_y = fabs (graphene_vec3_get_y (&matrix_scale));
      }
      return;
    }
}

gboolean
gsk_gpu_render_pass_push_transform (GskGpuRenderPass                 *self,
                                    GskTransform                     *transform,
                                    const graphene_rect_t            *bounds,
                                    const graphene_rect_t            *child_bounds,
                                    GskGpuRenderPassTransformStorage *storage)
{
  storage->modelview = self->modelview;
  storage->scale = self->scale;
  storage->offset = self->offset;
  gsk_gpu_clip_init_copy (&storage->clip, &self->clip);
  storage->modified = 0;

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
        gsk_gpu_clip_scale (&self->clip, &storage->clip, GDK_DIHEDRAL_NORMAL, scale_x, scale_y);
        self->offset.x = (self->offset.x + dx) / scale_x;
        self->offset.y = (self->offset.y + dy) / scale_y;
        graphene_vec2_init (&self->scale, scale_x, scale_y);
        graphene_vec2_multiply (&self->scale, &storage->scale, &self->scale);
        self->modelview = gsk_transform_ref (storage->modelview);
        storage->modified = GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;
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
        gsk_gpu_clip_scale (&self->clip, &storage->clip, inverted, scale_x, scale_y);
        self->offset.x = (self->offset.x + dx) / scale_x;
        self->offset.y = (self->offset.y + dy) / scale_y;
        self->offset = GRAPHENE_POINT_INIT (xx * self->offset.x + xy * self->offset.y,
                                            yx * self->offset.x + yy * self->offset.y);
        old_scale_x = graphene_vec2_get_x (&storage->scale);
        old_scale_y = graphene_vec2_get_y (&storage->scale);
        graphene_vec2_init (&self->scale,
                            fabs (scale_x * (old_scale_x * xx + old_scale_y * yx)),
                            fabs (scale_y * (old_scale_x * xy + old_scale_y * yy)));
        self->modelview = gsk_transform_dihedral (gsk_transform_ref (storage->modelview), dihedral);
        storage->modified = GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_MATRIX;
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
        else if (storage->clip.type == GSK_GPU_CLIP_NONE)
          {
            GskTransform *inverse;
            graphene_rect_t new_bounds;
            inverse = gsk_transform_invert (gsk_transform_ref (clip_transform));
            gsk_transform_transform_bounds (inverse, &storage->clip.rect.bounds, &new_bounds);
            gsk_transform_unref (inverse);
            gsk_gpu_clip_init_empty (&self->clip, graphene_point_zero (), &new_bounds);
          }
        else if (!gsk_gpu_clip_transform (&self->clip, &storage->clip, clip_transform, child_bounds))
          {
            gsk_transform_unref (clip_transform);
            return FALSE;
          }

        self->modelview = gsk_transform_scale (gsk_transform_ref (storage->modelview),
                                               graphene_vec2_get_x (&self->scale),
                                               graphene_vec2_get_y (&self->scale));
        self->modelview = gsk_transform_transform (self->modelview, clip_transform);
        gsk_transform_unref (clip_transform);

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
            dx /= scale_x;
            dy /= scale_y;
            self->offset = GRAPHENE_POINT_INIT (xx * dx + xy * dy,
                                                yx * dx + yy * dy);
            graphene_vec2_init (&self->scale,
                                fabs (scale_x * xx + scale_y * yx),
                                fabs (scale_x * xy + scale_y * yy));
            g_clear_pointer (&self->modelview, gsk_transform_unref);
            self->modelview = gsk_transform_dihedral (NULL, dihedral);
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
        else
          {
            float scale_x, scale_y, old_pixels, new_pixels;

            extract_scale_from_transform (self->modelview, &scale_x, &scale_y);

            old_pixels = MAX (graphene_vec2_get_x (&storage->scale) * storage->clip.rect.bounds.size.width,
                              graphene_vec2_get_y (&storage->scale) * storage->clip.rect.bounds.size.height);
            new_pixels = MAX (scale_x * self->clip.rect.bounds.size.width,
                              scale_y * self->clip.rect.bounds.size.height);

            /* Check that our offscreen doesn't get too big.  1.5 ~ sqrt(2) */
            if (new_pixels > 1.5 * old_pixels)
              {
                float forced_downscale = 2 * old_pixels / new_pixels;
                scale_x *= forced_downscale;
                scale_y *= forced_downscale;
              }

            self->modelview = gsk_transform_scale (self->modelview, 1 / scale_x, 1 / scale_y);
            graphene_vec2_init (&self->scale, scale_x, scale_y);
            self->offset = GRAPHENE_POINT_INIT (0, 0);
          }
        storage->modified = GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_MATRIX;
      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  self->pending_globals |= storage->modified;

  return TRUE;
}

void
gsk_gpu_render_pass_pop_transform (GskGpuRenderPass                 *self,
                                   GskGpuRenderPassTransformStorage *storage)
{
  g_clear_pointer (&self->modelview, gsk_transform_unref);
  self->modelview = storage->modelview;
  self->scale = storage->scale;
  self->offset = storage->offset;
  gsk_gpu_clip_init_copy (&self->clip, &storage->clip);
  self->pending_globals |= storage->modified;
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
gsk_gpu_render_pass_push_clip_rect (GskGpuRenderPass            *self,
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

void
gsk_gpu_render_pass_pop_clip_rect (GskGpuRenderPass            *self,
                                   GskGpuRenderPassClipStorage *storage)
{
  if (storage->modified & GSK_GPU_GLOBAL_SCISSOR)
    self->scissor = storage->scissor;

  if (storage->modified & GSK_GPU_GLOBAL_CLIP)
    gsk_gpu_clip_init_copy (&self->clip, &storage->clip);

  self->pending_globals |= storage->modified;
}

gboolean
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
      return FALSE;
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
  return TRUE;
}

void
gsk_gpu_render_pass_pop_clip_rounded (GskGpuRenderPass            *self,
                                      GskGpuRenderPassClipStorage *storage)
{
  /* They're identical currently */
  gsk_gpu_render_pass_pop_clip_rect (self, storage);
}

#include "config.h"

#include "gskgpuocclusionprivate.h"

#include "gskgpuclearopprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpunodeprocessorprivate.h"

#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gdkcairoprivate.h"

/* the amount of pixels for us to potentially save to warrant
 * carving out a rectangle for an extra render pass
 */
#define MIN_PIXELS_FOR_OCCLUSION_PASS 1000 * 100

/* the amount of the whole image for us to potentially save to warrant
 * carving out a rectangle for an extra render pass
 */
#define MIN_PERCENTAGE_FOR_OCCLUSION_PASS 10

struct _GskGpuOcclusion
{
  GskGpuFrame *frame;
  GskGpuImage *target;
  GdkColorState *target_color_state;
  GskRenderPassType pass_type;
  cairo_region_t *clip_region;
  graphene_rect_t viewport;

  cairo_rectangle_int_t device_clip;
  GskGpuTransform transform;

  GskGpuRenderPass pass;
  GskGpuRenderPassClipStorage scissor_storage;
  float background_color[4];

  guint has_background : 1;
  guint has_started_rendering : 1;
};

static gboolean G_GNUC_WARN_UNUSED_RESULT
gsk_gpu_occlusion_user_to_device (GskGpuOcclusion       *self,
                                  const graphene_rect_t *user,
                                  cairo_rectangle_int_t *device)
{
  graphene_rect_t tmp;
  gsk_gpu_transform_transform_rect (&self->transform,
                                    user,
                                    &tmp);
  return gsk_rect_to_cairo_shrink (&tmp, device);
}

static void
gsk_gpu_occlusion_init (GskGpuOcclusion       *self,
                        GskGpuFrame           *frame,
                        GskGpuImage           *target,
                        GdkColorState         *target_color_state,
                        GskRenderPassType      pass_type,
                        cairo_region_t        *clip_region,
                        const graphene_rect_t *viewport)
{
  self->frame = frame;
  self->target = target;
  self->target_color_state = target_color_state;
  self->pass_type = pass_type;
  self->clip_region = clip_region;
  self->viewport = *viewport;

  self->device_clip = (cairo_rectangle_int_t) { 0, 0, 0, 0 };
  gsk_gpu_transform_init (&self->transform,
                          GDK_DIHEDRAL_NORMAL,
                          &GRAPHENE_SIZE_INIT (gsk_gpu_image_get_width (target) / viewport->size.width,
                                               gsk_gpu_image_get_height (target) / viewport->size.height),
                          &GRAPHENE_POINT_INIT (-viewport->origin.x,
                                                -viewport->origin.y));

  self->has_background = FALSE;
  self->has_started_rendering = FALSE;
}

static void
gsk_gpu_occlusion_finish (GskGpuOcclusion *self)
{
  cairo_region_destroy (self->clip_region);

  if (self->has_started_rendering)
    {
      gsk_gpu_render_pass_finish (&self->pass);
    }
}

GskGpuFrame *
gsk_gpu_occlusion_get_frame (GskGpuOcclusion *self)
{
  return self->frame;
}

gboolean
gsk_gpu_occlusion_push_transform (GskGpuOcclusion *self,
                                  GskTransform    *transform,
                                  GskGpuTransform *out_save)
{
  *out_save = self->transform;
  
  return gsk_gpu_transform_transform (&self->transform, transform);
}

void
gsk_gpu_occlusion_pop_transform (GskGpuOcclusion       *self,
                                 const GskGpuTransform *saved)
{
  self->transform = *saved;
}

static gsize
gsk_gpu_occlusion_get_min_pixels (GskGpuOcclusion *self)
{
  gsize result;

  result = gsk_gpu_image_get_width (self->target) * gsk_gpu_image_get_height (self->target) *
           MIN_PERCENTAGE_FOR_OCCLUSION_PASS / 100;
  result = MAX (result, MIN_PIXELS_FOR_OCCLUSION_PASS);

  return result;
}

static gboolean
gsk_gpu_occlusion_clip (GskGpuOcclusion       *self,
                        const graphene_rect_t *clip)
{
  cairo_rectangle_int_t device;

  if (!gsk_gpu_occlusion_user_to_device (self, clip, &device))
    return FALSE;

  if (!gdk_rectangle_intersect (&device, &self->device_clip, &device))
    return FALSE;

  /* Only check pixel amount if the rect got smaller.
   * This way, we guarantee that covering the full rectangle
   * will always start an occlusion pass */
  if ((device.width < self->device_clip.width || device.height < self->device_clip.height) &&
      device.width * device.height < gsk_gpu_occlusion_get_min_pixels (self))
    return FALSE;

  self->device_clip = device;

  return TRUE;
}

static void
gsk_gpu_occlusion_begin_rendering (GskGpuOcclusion *self,
                                   float           *clear_color) /* float[4] or NULL */
{
  GskDebugProfile *profile;

  profile = gsk_gpu_frame_get_profile (self->frame);
  if (profile)
    {
      profile->self.n_bases++;
      profile->self.base_pixels += self->device_clip.width * self->device_clip.height;
    }

  if (self->has_started_rendering)
    {
      gsk_gpu_render_pass_push_clip_device_rect (&self->pass, &self->device_clip, &self->scissor_storage);
      gsk_gpu_render_pass_set_transform (&self->pass, &self->transform);

      if (clear_color &&
          (!self->has_background ||
           (self->has_background &&
            (clear_color[0] != self->background_color[0] ||
             clear_color[1] != self->background_color[1] ||
             clear_color[2] != self->background_color[2] ||
             clear_color[3] != self->background_color[3]))))
        {
          gsk_gpu_clear_op (self->pass.frame, &self->device_clip, clear_color);
        }
    }
  else
    {
      GskGpuLoadOp load_op;
      cairo_rectangle_int_t extents;

      if (!gdk_cairo_region_is_rectangle (self->clip_region))
        {
          self->has_background = FALSE;
          load_op = GSK_GPU_LOAD_OP_LOAD;
        }
      else if (clear_color)
        {
          self->background_color[0] = clear_color[0];
          self->background_color[1] = clear_color[1];
          self->background_color[2] = clear_color[2];
          self->background_color[3] = clear_color[3];
          self->has_background = TRUE;
          load_op = GSK_GPU_LOAD_OP_CLEAR;
        }
      else
        {
          self->has_background = FALSE;
          load_op = GSK_GPU_LOAD_OP_DONT_CARE;
        }

      cairo_region_get_extents (self->clip_region, &extents);
      self->has_started_rendering = TRUE;
      gsk_gpu_render_pass_init (&self->pass,
                                self->frame,
                                self->target,
                                self->target_color_state,
                                self->pass_type,
                                load_op,
                                clear_color,
                                &extents,
                                &self->viewport);

      gsk_gpu_render_pass_push_clip_device_rect (&self->pass, &self->device_clip, &self->scissor_storage);
      gsk_gpu_render_pass_set_transform (&self->pass, &self->transform);
      if (!self->has_background && clear_color)
        gsk_gpu_clear_op (self->pass.frame, &self->pass.scissor, clear_color);
    }
}

GskGpuRenderPass *
gsk_gpu_occlusion_begin_rendering_whatever (GskGpuOcclusion *self)
{
  gsk_gpu_occlusion_begin_rendering (self, NULL);

  return &self->pass;
}

GskGpuRenderPass *
gsk_gpu_occlusion_begin_rendering_transparent (GskGpuOcclusion *self)
{
  gsk_gpu_occlusion_begin_rendering (self, (float[4]) { 0, 0, 0, 0 });

  return &self->pass;
}

GskGpuRenderPass *
gsk_gpu_occlusion_begin_rendering_color (GskGpuOcclusion *self,
                                         const GdkColor  *color)
{
  GdkColor convert;

  gdk_color_convert (&convert, self->target_color_state, color);
  gsk_gpu_occlusion_begin_rendering (self, convert.values);

  return &self->pass;
}

GskGpuRenderPass *
gsk_gpu_occlusion_try_node_untracked (GskGpuOcclusion *self,
                                      GskRenderNode   *node)
{
  graphene_rect_t opaque;
  cairo_rectangle_int_t prev_clip;
  GskGpuRenderPass *result;

  /* This catches the corner cases of empty nodes, so after this check
   * there's quaranteed to be at least 1 pixel that needs to be drawn
   */
  if (node->bounds.size.width == 0 || node->bounds.size.height == 0 ||
      !gsk_render_node_get_opaque_rect (node, &opaque))
    return NULL;

  prev_clip = self->device_clip;
  if (!gsk_gpu_occlusion_clip (self, &opaque))
    return NULL;

  result = GSK_RENDER_NODE_GET_CLASS (node)->occlusion (node, self);

  if (result == NULL)
    {
      self->device_clip = prev_clip;
    }

  return result;
}

GskGpuRenderPass *
gsk_gpu_occlusion_try_node (GskGpuOcclusion *self,
                            GskRenderNode   *node,
                            gsize            pos)
{
  GskGpuRenderPass *result;

  gsk_gpu_frame_start_node (self->frame, node, pos);

  result = gsk_gpu_occlusion_try_node_untracked (self, node);
  
  gsk_gpu_frame_end_node (self->frame);

  return result;
}

static gboolean
gsk_gpu_occlusion_run (GskGpuOcclusion       *self,
                       cairo_rectangle_int_t *device_clip,
                       GskRenderNode         *node)
{
  gboolean result;

  self->device_clip = *device_clip;

  if (gsk_gpu_occlusion_try_node_untracked (self, node))
    result = TRUE;
  else
    {
      GskGpuRenderPass *pass;

      pass = gsk_gpu_occlusion_begin_rendering_transparent (self);
      gsk_gpu_node_processor_add_first_node_untracked (pass, node);
      result = FALSE;
    }
  
  /* NB: not the passed in device clip, we might have shrunk the region */
  cairo_region_subtract_rectangle (self->clip_region, &self->device_clip);
  gsk_gpu_render_pass_pop_clip_device_rect (&self->pass, &self->scissor_storage);

  return result;
}

void
gsk_gpu_occlusion_render_node (GskGpuFrame           *frame,
                               GskGpuImage           *target,
                               GdkColorState         *target_color_state,
                               GskRenderPassType      pass_type,
                               cairo_region_t        *clip,
                               const graphene_rect_t *viewport,
                               GskRenderNode         *node)
{
  GskGpuOcclusion self;
  int i, n, best, best_size;
  cairo_rectangle_int_t rect;
  gboolean do_culling;

  gsk_gpu_occlusion_init (&self,
                          frame,
                          target,
                          target_color_state,
                          pass_type,
                          clip,
                          viewport);

  do_culling = gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_OCCLUSION_CULLING);

  while (do_culling &&
         (n = cairo_region_num_rectangles (clip)) > 0)
    {
      best = -1;
      best_size = 0;
      for (i = 0; i < n; i++)
        {
          cairo_region_get_rectangle (clip, i, &rect);
          if (rect.width * rect.height > best_size)
            {
              best = i;
              best_size = rect.width * rect.height;
            }
        }

      if (best_size < MIN_PIXELS_FOR_OCCLUSION_PASS)
        break;

      cairo_region_get_rectangle (clip, best, &rect);
      if (!gsk_gpu_occlusion_run (&self, &rect, node))
        break;
    }

  while (cairo_region_num_rectangles (clip) > 0) 
    {
      cairo_region_get_rectangle (clip, 0, &rect);

      gsk_gpu_occlusion_run (&self, &rect, node);
    }

  gsk_gpu_occlusion_finish (&self);
}


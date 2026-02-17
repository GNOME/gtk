#include "config.h"

#include "gskgpunodeprocessorprivate.h"

#include "gskgpuarithmeticopprivate.h"
#include "gskgpuborderopprivate.h"
#include "gskgpuboxshadowopprivate.h"
#include "gskgpublendmodeopprivate.h"
#include "gskgpublendopprivate.h"
#include "gskgpublitopprivate.h"
#include "gskgpubluropprivate.h"
#include "gskgpucacheprivate.h"
#include "gskgpucachedglyphprivate.h"
#include "gskgpucachedfillprivate.h"
#include "gskgpucachedstrokeprivate.h"
#include "gskgpuclearopprivate.h"
#include "gskgpucolorizeopprivate.h"
#include "gskgpucolormatrixopprivate.h"
#include "gskgpucomponenttransferopprivate.h"
#include "gskgpucompositeopprivate.h"
#include "gskgpucoloropprivate.h"
#include "gskgpuconicgradientopprivate.h"
#include "gskgpuconvertbuiltinopprivate.h"
#include "gskgpuconvertcicpopprivate.h"
#include "gskgpuconvertopprivate.h"
#include "gskgpucrossfadeopprivate.h"
#include "gskgpudisplacementopprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuglobalsopprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpulineargradientopprivate.h"
#include "gskgpumaskopprivate.h"
#include "gskgpumipmapopprivate.h"
#include "gskgpuocclusionprivate.h"
#include "gskgpuradialgradientopprivate.h"
#include "gskgpurenderpassprivate.h"
#include "gskgpuroundedcoloropprivate.h"
#include "gskgpuscissoropprivate.h"
#include "gskgputextureopprivate.h"
#include "gskgpuuploadopprivate.h"
#include "gskgpuutilsprivate.h"

#include "gskarithmeticnodeprivate.h"
#include "gskblendnodeprivate.h"
#include "gskblurnode.h"
#include "gskbordernodeprivate.h"
#include "gskcairoblurprivate.h"
#include "gskclipnode.h"
#include "gskcolormatrixnodeprivate.h"
#include "gskcolornodeprivate.h"
#include "gskcomponenttransfernodeprivate.h"
#include "gskcomponenttransferprivate.h"
#include "gskcompositenode.h"
#include "gskconicgradientnodeprivate.h"
#include "gskcontainernodeprivate.h"
#include "gskcrossfadenode.h"
#include "gskdebugprivate.h"
#include "gskdebugnode.h"
#include "gskdisplacementnodeprivate.h"
#include "gskfillnode.h"
#include "gskinsetshadownodeprivate.h"
#include "gskisolationnode.h"
#include "gsklineargradientnodeprivate.h"
#include "gskmasknode.h"
#include "gskopacitynode.h"
#include "gskoutsetshadownodeprivate.h"
#include "gskpath.h"
#include "gskradialgradientnodeprivate.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrepeatnodeprivate.h"
#include "gskroundedclipnode.h"
#include "gskroundedrectprivate.h"
#include "gskshadownodeprivate.h"
#include "gskstrokenode.h"
#include "gsksubsurfacenode.h"
#include "gsktextnodeprivate.h"
#include "gsktexturenode.h"
#include "gsktexturescalenode.h"
#include "gsktransformnode.h"
#include "gsktransformprivate.h"
#include "gskprivate.h"

#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkcairoprivate.h"
#include "gdk/gdkmemorytextureprivate.h"
#include "gdk/gdkrgbaprivate.h"
#include "gdk/gdksubsurfaceprivate.h"
#include "gdk/gdktextureprivate.h"

/* the epsilon we allow pixels to be off due to rounding errors.
 * Chosen rather randomly.
 */
#define EPSILON 0.001

/* the amount of pixels for us to potentially save to warrant
 * carving out a rectangle for an extra render pass
 */
#define MIN_PIXELS_FOR_OCCLUSION_PASS 1000 * 100

/* the amount of the whole image for us to potentially save to warrant
 * carving out a rectangle for an extra render pass
 */
#define MIN_PERCENTAGE_FOR_OCCLUSION_PASS 10

/* A note about coordinate systems
 *
 * The rendering code keeps track of multiple coordinate systems to optimize rendering as
 * much as possible and in the coordinate system it makes most sense in.
 * Sometimes there are cases where GL requires a certain coordinate system, too.
 *
 * 1. the node coordinate system
 * This is the coordinate system of the rendernode. It is basically not used outside of
 * looking at the node and basically never hits the GPU (it does for paths). We immediately
 * convert it to:
 *
 * 2. the basic coordinate system
 * convert on CPU: NodeProcessor.offset
 * convert on GPU: ---
 * This is the coordinate system we emit vertex state in, the clip is tracked here.
 * The main benefit is that most transform nodes only change the offset, so we can avoid
 * updating any state in this coordinate system when that happens.
 *
 * 3. the scaled coordinate system
 * converts on CPU: NodeProcessor.scale
 * converts on GPU: GSK_GLOBAL_SCALE
 * This includes the current scale of the transform. It is usually equal to the scale factor
 * of the window we are rendering to (which is bad because devs without hidpi screens can
 * forget this and then everyone else will see bugs). We make decisions about pixel sizes in
 * this coordinate system, like picking glyphs from the glyph cache or the sizes of offscreens
 * for offscreen rendering.
 *
 * 4. the device coordinate system
 * converts on CPU: NodeProcessor.modelview
 * converts on GPU: ---
 * The scissor rect is tracked in this coordinate system. It represents the actual device pixels.
 * A bunch of optimizations (like glScissor() and glClear()) can be done here, so in the case
 * that modelview == NULL and we end up with integer coordinates (because pixels), we try to go
 * here.
 * This coordinate system does not exist on shaders as they rarely reason about pixels, and if
 * they need to, they can ask the fragment shader via gl_FragCoord.
 *
 * 5. the GL coordinate system
 * converts on CPU: NodeProcessor.projection
 * converts on GPU: GSK_GLOBAL_MVP (from scaled coordinate system)
 * This coordinate system is what GL (or Vulkan) expect coordinates to appear in, and is usually
 * (-1, -1) => (1, 1), but may be flipped etc depending on the render target. The CPU essentially
 * never uses it, other than to allow the vertex shaders to emit its vertices.
 */


typedef enum {
  /* The returned image will be sampled outside the bounds, so it is
   * important that it returns the right values.
   * In particular, opaque textures must ensure they return transparency
   * and images must not be contained in an atlas.
   */
  GSK_GPU_AS_IMAGE_SAMPLED_OUT_OF_BOUNDS = (1 << 0),
  /* The returned image needs to be the exact size of the given clip
   * rect, for example because it will be repeated.
   * In detail: out_bounds must equal clip_bounds
   */
  GSK_GPU_AS_IMAGE_EXACT_SIZE = (1 << 1),
} GskGpuAsImageFlags;

static void             gsk_gpu_node_processor_add_node_untracked       (GskGpuRenderPass            *self,
                                                                         GskRenderNode                  *node);
static GskGpuImage *    gsk_gpu_get_node_as_image                       (GskGpuFrame                    *frame,
                                                                         GskGpuAsImageFlags              flags,
                                                                         GdkColorState                  *ccs,
                                                                         const graphene_rect_t          *clip_bounds,
                                                                         const graphene_vec2_t          *scale,
                                                                         GskRenderNode                  *node,
                                                                         graphene_rect_t                *out_bounds);

static GskGpuImage *
create_offscreen_image (GskGpuFrame     *frame,
                        gboolean         with_mipmap,
                        GdkMemoryFormat  format,
                        gboolean         is_srgb,
                        gsize            width,
                        gsize            height)
{
  GskGpuImage *result;
  GskDebugProfile *profile;

  result = gsk_gpu_device_create_offscreen_image (gsk_gpu_frame_get_device (frame),
                                                  with_mipmap,
                                                  format,
                                                  is_srgb,
                                                  width,
                                                  height);
  if (result == NULL)
    return NULL;

  profile = gsk_gpu_frame_get_profile (frame);
  if (profile)
    {
      profile->self.n_offscreens++;
      profile->self.offscreen_pixels += width * height;
    }

  return result;
}
     
static GskGpuImage *
gsk_gpu_node_processor_init_draw (GskGpuRenderPass   *self,
                                  GskGpuFrame           *frame,
                                  GdkColorState         *ccs,
                                  GdkMemoryDepth         depth,
                                  const graphene_vec2_t *scale,
                                  const graphene_rect_t *viewport)
{
  GskGpuImage *image;
  cairo_rectangle_int_t area;

  area.x = 0;
  area.y = 0;
  area.width = MAX (1, ceilf (graphene_vec2_get_x (scale) * viewport->size.width - EPSILON));
  area.height = MAX (1, ceilf (graphene_vec2_get_y (scale) * viewport->size.height - EPSILON));

  image = create_offscreen_image (frame,
                                  FALSE,
                                  gdk_memory_depth_get_format (depth),
                                  gdk_memory_depth_is_srgb (depth),
                                  area.width, area.height);
  if (image == NULL)
    return NULL;

  gsk_gpu_render_pass_init (self,
                            frame,
                            image,
                            ccs,
                            GSK_RENDER_PASS_OFFSCREEN,
                            GSK_GPU_LOAD_OP_CLEAR,
                            GSK_VEC4_TRANSPARENT,
                            &area,
                            viewport);

  return image;
}

static gboolean G_GNUC_WARN_UNUSED_RESULT
gsk_gpu_node_processor_clip_node_bounds (GskGpuRenderPass *self,
                                         GskRenderNode       *node,
                                         graphene_rect_t     *out_bounds)
{
  graphene_rect_t tmp;

  if (!gsk_gpu_render_pass_get_clip_bounds (self, &tmp))
    return FALSE;
  
  if (!gsk_rect_intersection (&tmp, &node->bounds, out_bounds))
    return FALSE;

  return TRUE;
}

static gboolean G_GNUC_WARN_UNUSED_RESULT
gsk_gpu_node_processor_clip_node_bounds_and_snap_to_grid (GskGpuRenderPass *self,
                                                          GskRenderNode *node,
                                                          graphene_rect_t *out_bounds)
{
  graphene_rect_t tmp;

  if (!gsk_gpu_render_pass_get_clip_bounds (self, &tmp))
    return FALSE;

  if (!gsk_rect_intersection (&tmp, &node->bounds, out_bounds))
    return FALSE;

  if (!gsk_rect_snap_to_grid (out_bounds, &self->scale, &self->offset, out_bounds))
    return FALSE;

  return TRUE;
}

static GdkColorState *
gsk_gpu_get_acs_for_builtin (GdkColorState *builtin)
{
  switch (GDK_BUILTIN_COLOR_STATE_ID (builtin))
    {
    case GDK_BUILTIN_COLOR_STATE_ID_OKLAB:
    case GDK_BUILTIN_COLOR_STATE_ID_OKLCH:
      return GDK_COLOR_STATE_SRGB_LINEAR;

    case GDK_BUILTIN_COLOR_STATE_N_IDS:
    default:
      g_assert_not_reached ();
      return NULL;
    }
}

static GdkColorState *
gsk_gpu_get_acs_for_cicp (GdkColorState *cicp,
                          GdkColorState *ccs)
{
  switch (GDK_DEFAULT_COLOR_STATE_ID (ccs))
    {
      case GDK_COLOR_STATE_ID_SRGB:
      case GDK_COLOR_STATE_ID_SRGB_LINEAR:
        return GDK_COLOR_STATE_SRGB_LINEAR;

      case GDK_COLOR_STATE_ID_REC2100_PQ:
      case GDK_COLOR_STATE_ID_REC2100_LINEAR:
        return GDK_COLOR_STATE_REC2100_LINEAR;

    case GDK_COLOR_STATE_N_IDS:
    default:
      g_assert_not_reached ();
      return NULL;
    }
}

static void
gsk_gpu_node_processor_image_op (GskGpuRenderPass   *self,
                                 GskGpuImage           *image,
                                 GdkColorState         *image_color_state,
                                 GskGpuSampler          sampler,
                                 const graphene_rect_t *rect,
                                 const graphene_rect_t *tex_rect)
{
  GskGpuImage *copy = NULL;

  if (GDK_IS_BUILTIN_COLOR_STATE (image_color_state))
    {
      gsk_gpu_convert_builtin_op (self,
                                  self->ccs,
                                  gsk_gpu_get_acs_for_builtin (image_color_state),
                                  rect,
                                  image,
                                  sampler,
                                  GDK_BUILTIN_COLOR_STATE_ID (image_color_state),
                                  FALSE,
                                  FALSE,
                                  tex_rect);
    }
  else if (!GDK_IS_DEFAULT_COLOR_STATE (image_color_state))
    {
      const GdkCicp *cicp = gdk_color_state_get_cicp (image_color_state);

      g_assert (cicp != NULL);

      gsk_gpu_convert_cicp_op (self,
                               self->ccs,
                               gsk_gpu_get_acs_for_cicp (image_color_state, self->ccs),
                               rect,
                               image,
                               sampler,
                               TRUE,
                               FALSE,
                               tex_rect,
                               cicp->color_primaries,
                               cicp->transfer_function,
                               cicp->matrix_coefficients,
                               cicp->range == GDK_CICP_RANGE_NARROW ? 0 : 1);
    }
  else if (gsk_gpu_image_get_shader_op (image) != GDK_SHADER_DEFAULT ||
           self->opacity < 1.0 ||
           !gdk_color_state_equal (image_color_state, self->ccs))
    {
      gsk_gpu_convert_op (self,
                          self->ccs,
                          TRUE,
                          image_color_state,
                          rect,
                          image,
                          sampler,
                          tex_rect);
    }
  else
    {
      gsk_gpu_texture_op (self,
                          self->ccs,
                          rect,
                          image,
                          sampler,
                          tex_rect);
    }

  g_clear_object (&copy);
}

static GskGpuImage *
gsk_gpu_node_processor_create_offscreen (GskGpuFrame           *frame,
                                         GdkColorState         *ccs,
                                         const graphene_vec2_t *scale,
                                         const graphene_rect_t *viewport,
                                         GskRenderNode         *node)
{
  GskGpuImage *image;
  cairo_rectangle_int_t area;
  GdkMemoryDepth depth;

  area.x = 0;
  area.y = 0;
  area.width = MAX (1, ceilf (graphene_vec2_get_x (scale) * viewport->size.width - EPSILON));
  area.height = MAX (1, ceilf (graphene_vec2_get_y (scale) * viewport->size.height - EPSILON));

  depth = gdk_memory_depth_merge (gdk_color_state_get_depth (ccs),
                                  gsk_render_node_get_preferred_depth (node));

  image = create_offscreen_image (frame,
                                  FALSE,
                                  gdk_memory_depth_get_format (depth),
                                  gdk_memory_depth_is_srgb (depth),
                                  area.width, area.height);
  if (image == NULL)
    return NULL;

  gsk_gpu_node_processor_process (frame,
                                  image,
                                  ccs,
                                  cairo_region_create_rectangle (&area),
                                  node,
                                  viewport,
                                  GSK_RENDER_PASS_OFFSCREEN);

  return image;
}

static void
gsk_gpu_node_processor_add_node (GskGpuRenderPass *self,
                                 GskRenderNode       *node,
                                 gsize                pos)
{
  gsk_gpu_frame_start_node (self->frame, node, pos);

  gsk_gpu_node_processor_add_node_untracked (self, node);

  gsk_gpu_frame_end_node (self->frame);
}

static GskGpuImage *
gsk_gpu_get_node_as_image_via_offscreen (GskGpuFrame           *frame,
                                         GskGpuAsImageFlags     flags,
                                         GdkColorState         *ccs,
                                         const graphene_rect_t *clip_bounds,
                                         const graphene_vec2_t *scale,
                                         GskRenderNode         *node,
                                         graphene_rect_t       *out_bounds)
{
  GskGpuImage *result;

  GSK_DEBUG (FALLBACK, "Offscreening node '%s'", g_type_name_from_instance ((GTypeInstance *) node));
  result = gsk_gpu_node_processor_create_offscreen (frame,
                                                    ccs,
                                                    scale,
                                                    clip_bounds,
                                                    node);

  *out_bounds = *clip_bounds;
  return result;
}

/*
 * gsk_gpu_node_copy_image:
 * @frame: The frame the image will be copied in
 * @ccs: color state the copy will be in
 * @image: (transfer full): The image to copy
 * @prepare_mipmap: If the copied image should reserve space for
 *   mipmaps
 *
 * Generates a copy of @image, but makes the copy premultiplied and potentially
 * reserves space for mipmaps.
 *
 * Returns: (transfer full): The copy of the image.
 **/
static GskGpuImage *
gsk_gpu_copy_image (GskGpuFrame   *frame,
                    GdkColorState *ccs,
                    GskGpuImage   *image,
                    GdkColorState *image_cs,
                    gboolean       prepare_mipmap)
{
  GskGpuImage *copy;
  gsize width, height;
  GskGpuImageFlags flags;
  GdkMemoryDepth depth;

  width = gsk_gpu_image_get_width (image);
  height = gsk_gpu_image_get_height (image);
  flags = gsk_gpu_image_get_flags (image);
  depth = gdk_memory_format_get_depth (gsk_gpu_image_get_format (image),
                                       gsk_gpu_image_get_conversion (image) == GSK_GPU_CONVERSION_SRGB);
  depth = gdk_memory_depth_merge (depth, gdk_color_state_get_depth (ccs));

  copy = create_offscreen_image (frame,
                                 prepare_mipmap,
                                 gdk_memory_depth_get_format (depth),
                                 gdk_memory_depth_is_srgb (depth),
                                 width, height);

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_BLIT) &&
      (flags & (GSK_GPU_IMAGE_BLIT | GSK_GPU_IMAGE_FILTERABLE)) == (GSK_GPU_IMAGE_FILTERABLE | GSK_GPU_IMAGE_BLIT) &&
      gsk_gpu_image_get_shader_op (image) == GDK_SHADER_DEFAULT &&
      gdk_color_state_equal (ccs, image_cs))
    {
      gsk_gpu_blit_op (frame,
                       image,
                       copy,
                       &(cairo_rectangle_int_t) { 0, 0, width, height },
                       &(cairo_rectangle_int_t) { 0, 0, width, height },
                       GSK_GPU_BLIT_NEAREST);
    }
  else
    {
      GskGpuRenderPass other;
      graphene_rect_t rect = GRAPHENE_RECT_INIT (0, 0, width, height);
      GskGpuRenderPassBlendStorage storage;

      gsk_gpu_render_pass_init (&other,
                                frame,
                                copy,
                                ccs,
                                GSK_RENDER_PASS_OFFSCREEN,
                                GSK_GPU_LOAD_OP_DONT_CARE,
                                NULL,
                                &(cairo_rectangle_int_t) { 0, 0, width, height },
                                &rect);

      gsk_gpu_render_pass_push_blend (&other, GSK_GPU_BLEND_NONE, &storage);

      gsk_gpu_node_processor_image_op (&other,
                                       image,
                                       image_cs,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &rect,
                                       &rect);

      gsk_gpu_render_pass_pop_blend (&other, &storage);
      gsk_gpu_render_pass_finish (&other);
    }

  g_object_unref (image);

  return copy;
}

static GskGpuImage *
gsk_gpu_node_processor_get_node_as_image_untracked (GskGpuRenderPass   *self,
                                                    GskGpuAsImageFlags     flags,
                                                    const graphene_rect_t *clip_bounds,
                                                    GskRenderNode         *node,
                                                    graphene_rect_t       *out_bounds)
{
  graphene_rect_t clip;

  if (flags & GSK_GPU_AS_IMAGE_EXACT_SIZE)
    {
      if (clip_bounds == NULL)
        clip = node->bounds;
      else
        clip = *clip_bounds;
    }
  else
    {
      if (clip_bounds == NULL)
        {
          if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip))
            return NULL;
        }
      else
        {
          if (!gsk_rect_intersection (clip_bounds, &node->bounds, &clip))
            return NULL;
        }
      if (!gsk_rect_snap_to_grid (&clip, &self->scale, &self->offset, &clip))
        return NULL;
    }

  return gsk_gpu_get_node_as_image (self->frame,
                                    flags,
                                    self->ccs,
                                    &clip,
                                    &self->scale,
                                    node,
                                    out_bounds);
}

/*
 * gsk_gpu_node_processor_get_node_as_image:
 * @self: a node processor
 * @flags: flags for the image
 * @clip_bounds: (nullable): clip rectangle to use or NULL to use
 *   the current clip
 * @node: the node to turn into an image
 * @pos: position of the node in the parent for tracking purposes or
 *   -1 to not do tracking
 * @out_bounds: bounds of the the image in node space
 *
 * Generates an image for the given node. The image is restricted to the
 * region in the clip bounds.
 *
 * The resulting image is guaranteed to be premultiplied.
 *
 * Returns: (nullable): The node as an image or %NULL if the node is fully
 *     clipped
 **/
static GskGpuImage *
gsk_gpu_node_processor_get_node_as_image (GskGpuRenderPass   *self,
                                          GskGpuAsImageFlags     flags,
                                          const graphene_rect_t *clip_bounds,
                                          GskRenderNode         *node,
                                          gsize                  pos,
                                          graphene_rect_t       *out_bounds)
{
  GskGpuImage *result;

  gsk_gpu_frame_start_node (self->frame, node, pos);

  result = gsk_gpu_node_processor_get_node_as_image_untracked (self,
                                                               flags,
                                                               clip_bounds,
                                                               node,
                                                               out_bounds);

  gsk_gpu_frame_end_node (self->frame);

  return result;
}

static void
gsk_gpu_node_processor_blur_op (GskGpuRenderPass       *self,
                                const graphene_rect_t     *rect,
                                const graphene_point_t    *shadow_offset,
                                float                      blur_radius,
                                const GdkColor            *shadow_color,
                                GskGpuImage               *source_image,
                                GdkMemoryDepth             source_depth,
                                const graphene_rect_t     *source_rect)
{
  GskGpuRenderPass other;
  GskGpuImage *intermediate;
  graphene_vec2_t direction;
  graphene_rect_t clip_rect, intermediate_rect;
  float clip_radius;
  GskGpuRenderPassTranslateStorage storage;

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius / 2.0);

  /* FIXME: Handle clip radius growing the clip too much */
  if (!gsk_gpu_render_pass_get_clip_bounds (self, &clip_rect))
    return;
  clip_rect.origin.x -= shadow_offset->x;
  clip_rect.origin.y -= shadow_offset->y;
  graphene_rect_inset (&clip_rect, 0.f, -clip_radius);
  if (!gsk_rect_intersection (rect, &clip_rect, &intermediate_rect))
    return;

  if (!gsk_rect_snap_to_grid (&intermediate_rect, &self->scale, &self->offset, &intermediate_rect))
    return;

  intermediate = gsk_gpu_node_processor_init_draw (&other,
                                                   self->frame,
                                                   self->ccs,
                                                   source_depth,
                                                   &self->scale,
                                                   &intermediate_rect);
  g_return_if_fail (intermediate != NULL);

  graphene_vec2_init (&direction, blur_radius, 0.0f);
  gsk_gpu_blur_op (&other,
                   other.ccs,
                   other.ccs,
                   &intermediate_rect,
                   source_image,
                   GSK_GPU_SAMPLER_TRANSPARENT,
                   FALSE,
                   &intermediate_rect,
                   &(GdkColor) { .color_state = other.ccs, .values = { 1, 1, 1, 1 } }, /* doesn't matter */
                   source_rect,
                   &direction);

  gsk_gpu_render_pass_finish (&other);

  gsk_gpu_render_pass_push_translate (self, shadow_offset, &storage);
  graphene_vec2_init (&direction, 0.0f, blur_radius);
  if (shadow_color)
    {
      gsk_gpu_blur_op (self,
                       self->ccs,
                       gsk_gpu_color_states_find (self->ccs, shadow_color),
                       rect,
                       intermediate,
                       GSK_GPU_SAMPLER_TRANSPARENT,
                       TRUE,
                       rect,
                       shadow_color,
                       &intermediate_rect,
                       &direction);
    }
  else
    {
      gsk_gpu_blur_op (self,
                       self->ccs,
                       self->ccs,
                       rect,
                       intermediate,
                       GSK_GPU_SAMPLER_TRANSPARENT,
                       FALSE,
                       rect,
                       &(GdkColor) { .color_state = other.ccs, .values = { 1, 1, 1, 1 } }, /* doesn't matter */
                       &intermediate_rect,
                       &direction);
    }
  gsk_gpu_render_pass_pop_translate (self, &storage);

  g_object_unref (intermediate);
}

static void
gsk_gpu_node_processor_add_cairo_node (GskGpuRenderPass *self,
                                       GskRenderNode       *node)
{
  GskGpuImage *image;
  graphene_rect_t clipped_bounds;

  if (!gsk_gpu_node_processor_clip_node_bounds_and_snap_to_grid (self, node, &clipped_bounds))
    return;

  image = gsk_gpu_upload_cairo_op (self->frame,
                                   &self->scale,
                                   &clipped_bounds,
                                   (GskGpuCairoFunc) gsk_render_node_draw_fallback,
                                   gsk_render_node_ref (node),
                                   (GDestroyNotify) gsk_render_node_unref);

  gsk_gpu_node_processor_image_op (self,
                                   image,
                                   GDK_COLOR_STATE_SRGB,
                                   GSK_GPU_SAMPLER_DEFAULT,
                                   &node->bounds,
                                   &clipped_bounds);
}

static void
gsk_gpu_node_processor_add_with_offscreen (GskGpuRenderPass *self,
                                           GskRenderNode       *node)
{
  GskGpuImage *image;
  graphene_rect_t tex_rect;

  image = gsk_gpu_node_processor_get_node_as_image_untracked (self,
                                                              0,
                                                              NULL,
                                                              node,
                                                              &tex_rect);
  if (image == NULL)
    return;

  gsk_gpu_node_processor_image_op (self,
                                   image,
                                   self->ccs,
                                   GSK_GPU_SAMPLER_DEFAULT,
                                   &node->bounds,
                                   &tex_rect);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_node_clipped (GskGpuRenderPass   *self,
                                         GskRenderNode         *node,
                                         gsize                  pos,
                                         const graphene_rect_t *clip_bounds)
{
  GskGpuRenderPassClipStorage storage;

  if (!gsk_gpu_render_pass_push_clip_rect (self, clip_bounds, &storage))
    {
      GskGpuImage *image;
      graphene_rect_t bounds, tex_rect;

      if (gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds) &&
          gsk_rect_intersection (&bounds, clip_bounds, &bounds))
        image = gsk_gpu_node_processor_get_node_as_image (self,
                                                          0,
                                                          &bounds,
                                                          node,
                                                          pos,
                                                          &tex_rect);
      else
        image = NULL;
      if (image)
        {
          gsk_gpu_node_processor_image_op (self,
                                           image,
                                           self->ccs,
                                           GSK_GPU_SAMPLER_DEFAULT,
                                           &bounds,
                                           &tex_rect);
          g_object_unref (image);
        }
      return;
    }

  if (!gsk_gpu_render_pass_is_all_clipped (self))
    gsk_gpu_node_processor_add_node (self, node, pos);

  gsk_gpu_render_pass_pop_clip_rect (self, &storage);
}

static void
gsk_gpu_node_processor_add_clip_node (GskGpuRenderPass *self,
                                      GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_node_clipped (self,
                                           gsk_clip_node_get_child (node),
                                           0,
                                           gsk_clip_node_get_clip (node));
}

static void
gsk_gpu_node_processor_add_rounded_clip_node_with_mask (GskGpuRenderPass *self,
                                                        GskRenderNode       *node)
{
  GskGpuRenderPass other;
  graphene_rect_t clip_bounds, child_rect;
  GskGpuImage *child_image, *mask_image;
  GdkColor white;

  if (!gsk_gpu_node_processor_clip_node_bounds_and_snap_to_grid (self, node, &clip_bounds))
    return;

  child_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                          0,
                                                          &clip_bounds,
                                                          gsk_rounded_clip_node_get_child (node),
                                                          0,
                                                          &child_rect);
  if (child_image == NULL)
    return;

  mask_image = gsk_gpu_node_processor_init_draw (&other,
                                                 self->frame,
                                                 self->ccs,
                                                 gdk_memory_depth_merge (gdk_color_state_get_depth (self->ccs),
                                                                         gsk_render_node_get_preferred_depth (node)),
                                                 &self->scale,
                                                 &clip_bounds);

  g_return_if_fail (mask_image != NULL);

  gdk_color_init (&white, self->ccs, ((float[]){ 1, 1, 1, 1 }));
  gsk_gpu_rounded_color_op (&other,
                            self->ccs,
                            self->ccs,
                            &node->bounds,
                            gsk_rounded_clip_node_get_clip (node),
                            &white);
  gsk_gpu_render_pass_finish (&other);

  gsk_gpu_mask_op (self,
                   self->ccs,
                   &clip_bounds,
                   child_image,
                   GSK_GPU_SAMPLER_DEFAULT,
                   mask_image,
                   GSK_GPU_SAMPLER_DEFAULT,
                   GSK_MASK_MODE_ALPHA,
                   &child_rect,
                   &clip_bounds);

  g_object_unref (child_image);
  g_object_unref (mask_image);
  gdk_color_finish (&white);
}

static void
gsk_gpu_node_processor_add_rounded_clip_node (GskGpuRenderPass *self,
                                              GskRenderNode       *node)
{
  GskGpuRenderPassClipStorage storage;
  const GskRoundedRect *clip;
  GskRenderNode *child;

  child = gsk_rounded_clip_node_get_child (node);
  clip = gsk_rounded_clip_node_get_clip (node);

  /* Common case for entries etc: rounded solid color background.
   * And we have a shader for that */
  if (gsk_render_node_get_node_type (child) == GSK_COLOR_NODE &&
      gsk_rect_contains_rect (&child->bounds, &clip->bounds))
    {
      const GdkColor *color;

      color = gsk_color_node_get_gdk_color (child);

      gsk_gpu_rounded_color_op (self,
                                self->ccs,
                                gsk_gpu_color_states_find (self->ccs, color),
                                &clip->bounds,
                                clip,
                                color);
      return;
    }

  if (!gsk_gpu_render_pass_push_clip_rounded (self, clip, &storage))
    {
      gsk_gpu_node_processor_add_rounded_clip_node_with_mask (self, node);
      return;
    }

  if (!gsk_gpu_render_pass_is_all_clipped (self))
    gsk_gpu_node_processor_add_node (self, child, 0);

  gsk_gpu_render_pass_pop_clip_rounded (self, &storage);
}

static void
gsk_gpu_node_processor_add_transform_node (GskGpuRenderPass *self,
                                           GskRenderNode       *node)
{
  GskRenderNode *child;
  GskTransform *transform;

  child = gsk_transform_node_get_child (node);
  transform = gsk_transform_node_get_transform (node);

  switch (gsk_transform_get_fine_category (transform))
    {
    case GSK_FINE_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        GskGpuRenderPassTranslateStorage storage;
        float dx, dy;

        gsk_transform_to_translate (transform, &dx, &dy);
        gsk_gpu_render_pass_push_translate (self, &GRAPHENE_POINT_INIT (dx, dy), &storage);
        gsk_gpu_node_processor_add_node (self, child, 0);
        gsk_gpu_render_pass_pop_translate (self, &storage);
      }
      break;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL:
    case GSK_FINE_TRANSFORM_CATEGORY_2D:
    case GSK_FINE_TRANSFORM_CATEGORY_3D:
    case GSK_FINE_TRANSFORM_CATEGORY_ANY:
    case GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN:
      {
        GskGpuRenderPassTransformStorage storage;

        if (!gsk_gpu_render_pass_push_transform (self,
                                                 transform,
                                                 &node->bounds,
                                                 &child->bounds,
                                                 &storage))
          {
            GskGpuImage *image;
            graphene_rect_t tex_rect;
            /* This cannot loop because the next time we'll hit the branch above */
            image = gsk_gpu_node_processor_get_node_as_image_untracked (self,
                                                                        0,
                                                                        NULL,
                                                                        node,
                                                                        &tex_rect);
            if (image != NULL)
              {
                gsk_gpu_node_processor_image_op (self,
                                                 image,
                                                 self->ccs,
                                                 GSK_GPU_SAMPLER_DEFAULT,
                                                 &node->bounds,
                                                 &tex_rect);
                g_object_unref (image);
              }
            return;
          }

          if (!gsk_gpu_render_pass_is_all_clipped (self))
            gsk_gpu_node_processor_add_node (self, child, 0);
          gsk_gpu_render_pass_pop_transform (self, &storage);
      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gsk_gpu_node_processor_add_opacity_node (GskGpuRenderPass *self,
                                         GskRenderNode       *node)
{
  GskRenderNode *child;
  float old_opacity = self->opacity;

  self->opacity *= gsk_opacity_node_get_opacity (node);

  child = gsk_opacity_node_get_child (node);

  gsk_gpu_frame_start_node (self->frame, child, 0);

  if (gsk_render_node_clears_background (child))
    gsk_gpu_node_processor_add_with_offscreen (self, child);
  else
    gsk_gpu_node_processor_add_node_untracked (self, child);

  gsk_gpu_frame_end_node (self->frame);

  self->opacity = old_opacity;
}

static void
gsk_gpu_node_processor_add_color_node (GskGpuRenderPass *self,
                                       GskRenderNode       *node)
{
  cairo_rectangle_int_t device;
  graphene_rect_t bounds, cover;
  const GdkColor *color;

  color = gsk_color_node_get_gdk_color (node);
  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds))
    return;

  if (gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_CLEAR) &&
      !self->modelview && 
      self->opacity >= 1.0 &&
      gdk_color_is_opaque (color) &&
      gsk_gpu_clip_get_largest_cover (&self->clip, &self->offset, &bounds, &cover) &&
      gsk_gpu_render_pass_user_to_device_shrink (self, &cover, &device) &&
      gdk_rectangle_intersect (&device, &self->scissor, &device) &&
      device.width * device.height > 100 * 100 && /* not worth the effort for small images */
      gsk_gpu_render_pass_device_to_user (self, &device, &cover))
    {
      float clear_color[4];

      if (bounds.origin.x != cover.origin.x)
        gsk_gpu_color_op (self,
                          self->ccs,
                          gsk_gpu_color_states_find (self->ccs, color),
                          &GRAPHENE_RECT_INIT (bounds.origin.x,
                                               bounds.origin.y,
                                               cover.origin.x - bounds.origin.x,
                                               bounds.size.height),
                          color);
      if (bounds.origin.y != cover.origin.y)
        gsk_gpu_color_op (self,
                          self->ccs,
                          gsk_gpu_color_states_find (self->ccs, color),
                          &GRAPHENE_RECT_INIT (bounds.origin.x,
                                               bounds.origin.y,
                                               bounds.size.width,
                                               cover.origin.y - bounds.origin.y),
                          color);
      if (bounds.origin.x + bounds.size.width != cover.origin.x + cover.size.width)
        gsk_gpu_color_op (self,
                          self->ccs,
                          gsk_gpu_color_states_find (self->ccs, color),
                          &GRAPHENE_RECT_INIT (cover.origin.x + cover.size.width,
                                               bounds.origin.y,
                                               bounds.origin.x + bounds.size.width - cover.origin.x - cover.size.width,
                                               bounds.size.height),
                          color);
      if (bounds.origin.y + bounds.size.height != cover.origin.y + cover.size.height)
        gsk_gpu_color_op (self,
                          self->ccs,
                          gsk_gpu_color_states_find (self->ccs, color),
                          &GRAPHENE_RECT_INIT (bounds.origin.x,
                                               cover.origin.y + cover.size.height,
                                               bounds.size.width,
                                               bounds.origin.y + bounds.size.height - cover.origin.y - cover.size.height),
                          color);

      gdk_color_to_float (color, self->ccs, clear_color);
      gsk_gpu_clear_op (self->frame, &device, clear_color);
    }
  else
    {
      gsk_gpu_color_op (self,
                        self->ccs,
                        gsk_gpu_color_states_find (self->ccs, color),
                        &bounds,
                        color);
    }
}

static void
gsk_gpu_node_processor_add_border_node (GskGpuRenderPass *self,
                                        GskRenderNode       *node)
{
  GdkColorState *acs;
  const GdkColor *colors;
  graphene_vec4_t widths;

  colors = gsk_border_node_get_gdk_colors (node);
  acs = gsk_gpu_color_states_find (self->ccs, &colors[0]);
  graphene_vec4_init_from_float (&widths, gsk_border_node_get_widths (node));

  gsk_gpu_border_op (self,
                     self->ccs,
                     acs,
                     &node->bounds,
                     gsk_border_node_get_outline (node),
                     &colors[0],
                     &colors[1],
                     &colors[2],
                     &colors[3],
                     &widths,
                     graphene_vec2_zero ());
}

static gboolean
texture_node_should_mipmap (GskRenderNode         *node,
                            GskGpuFrame           *frame,
                            const graphene_vec2_t *scale)
{
  GdkTexture *texture;

  texture = gsk_texture_node_get_texture (node);

  if (!gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_MIPMAP))
    return FALSE;

  return gdk_texture_get_width (texture) > 2 * node->bounds.size.width * graphene_vec2_get_x (scale) ||
         gdk_texture_get_height (texture) > 2 * node->bounds.size.height * graphene_vec2_get_y (scale);
}

static GskGpuImage *
gsk_gpu_lookup_texture (GskGpuFrame    *frame,
                        GdkColorState  *ccs,
                        GdkTexture     *texture,
                        gboolean        try_mipmap,
                        GdkColorState **out_image_cs)
{
  GskGpuCache *cache;
  GdkColorState *image_cs;
  GskGpuImage *image;

  cache = gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (frame));

  image = gsk_gpu_cache_lookup_texture_image (cache, texture, ccs);
  if (image)
    {
      *out_image_cs = ccs;
      return image;
    }

  image = gsk_gpu_cache_lookup_texture_image (cache, texture, NULL);
  if (image == NULL)
    image = gsk_gpu_frame_upload_texture (frame, try_mipmap, texture);

  /* Happens ie for oversized textures */
  if (image == NULL)
    return NULL;

  image_cs = gsk_gpu_color_state_apply_conversion (gdk_texture_get_color_state (texture),
                                                   gsk_gpu_image_get_conversion (image));
  g_assert (image_cs);

  *out_image_cs = image_cs;
  return image;
}

static GskGpuSampler
gsk_gpu_sampler_for_scaling_filter (GskScalingFilter scaling_filter)
{
  switch (scaling_filter)
    {
      case GSK_SCALING_FILTER_LINEAR:
        return GSK_GPU_SAMPLER_DEFAULT;

      case GSK_SCALING_FILTER_NEAREST:
        return GSK_GPU_SAMPLER_NEAREST;

      case GSK_SCALING_FILTER_TRILINEAR:
        return GSK_GPU_SAMPLER_MIPMAP_DEFAULT;

      default:
        g_assert_not_reached ();
        return GSK_GPU_SAMPLER_DEFAULT;
    }
}

/* must be set up with BLEND_ADD to avoid seams */
static void
gsk_gpu_node_processor_draw_texture_tiles (GskGpuRenderPass    *self,
                                           const graphene_rect_t  *texture_bounds,
                                           GdkTexture             *texture,
                                           GskScalingFilter        scaling_filter)
{
  GskGpuCache *cache;
  GskGpuDevice *device;
  GskGpuImage *tile;
  GdkColorState *tile_cs;
  GskGpuSampler sampler;
  gboolean need_mipmap;
  GdkMemoryTexture *memtex;
  GdkTexture *subtex;
  float scale_factor, scaled_tile_width, scaled_tile_height;
  gsize tile_size, width, height, n_width, n_height, x, y;
  graphene_rect_t clip_bounds;
  guint lod_level;

  device = gsk_gpu_frame_get_device (self->frame);
  cache = gsk_gpu_device_get_cache (device);
  sampler = gsk_gpu_sampler_for_scaling_filter (scaling_filter);
  need_mipmap = scaling_filter == GSK_SCALING_FILTER_TRILINEAR;
  if (!gsk_gpu_render_pass_get_clip_bounds (self, &clip_bounds))
    return;
  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  tile_size = gsk_gpu_device_get_tile_size (device);
  scale_factor = MIN (width / MAX (tile_size, texture_bounds->size.width),
                      height / MAX (tile_size, texture_bounds->size.height));
  if (scale_factor <= 1.0)
    lod_level = 0;
  else
    lod_level = floor (log2f (scale_factor));
  tile_size <<= lod_level;
  n_width = (width + tile_size - 1) / tile_size;
  n_height = (height + tile_size - 1) / tile_size;
  scaled_tile_width = texture_bounds->size.width * tile_size / width;
  scaled_tile_height = texture_bounds->size.height * tile_size / height;

  memtex = NULL;
  for (y = 0; y < n_height; y++)
    {
      for (x = 0; x < n_width; x++)
        {
          graphene_rect_t tile_rect = GRAPHENE_RECT_INIT (texture_bounds->origin.x + scaled_tile_width * x,
                                                          texture_bounds->origin.y + scaled_tile_height * y,
                                                          scaled_tile_width,
                                                          scaled_tile_height);
          if (!gsk_rect_intersection (&tile_rect, texture_bounds, &tile_rect) ||
              !gsk_rect_intersects (&clip_bounds, &tile_rect))
            continue;

          tile = gsk_gpu_cache_lookup_tile (cache, texture, lod_level, scaling_filter, y * n_width + x, &tile_cs);

          if (tile == NULL)
            {
              if (memtex == NULL)
                memtex = gdk_memory_texture_from_texture (texture);
              subtex = gdk_memory_texture_new_subtexture (memtex,
                                                          x * tile_size,
                                                          y * tile_size,
                                                          MIN (tile_size, width - x * tile_size),
                                                          MIN (tile_size, height - y * tile_size));
              tile = gsk_gpu_upload_texture_op_try (self->frame, need_mipmap, lod_level, scaling_filter, subtex);
              g_object_unref (subtex);
              if (tile == NULL)
                {
                  g_warning ("failed to create %zux%zu tile for %zux%zu texture. Out of memory?",
                             tile_size, tile_size, width, height);
                  goto out;
                }

              tile_cs = gdk_texture_get_color_state (texture);
              if (gsk_gpu_image_get_conversion (tile) == GSK_GPU_CONVERSION_SRGB)
                {
                  tile_cs = gdk_color_state_get_no_srgb_tf (tile_cs);
                  g_assert (tile_cs);
                }

              gsk_gpu_cache_cache_tile (cache, texture, lod_level, scaling_filter, y * n_width + x, tile, tile_cs);
            }

          if (need_mipmap &&
              (gsk_gpu_image_get_shader_op (tile) != GDK_SHADER_DEFAULT ||
               ((gsk_gpu_image_get_flags (tile) & GSK_GPU_IMAGE_CAN_MIPMAP)) != GSK_GPU_IMAGE_CAN_MIPMAP))
            {
              tile = gsk_gpu_copy_image (self->frame, self->ccs, tile, tile_cs, TRUE);
              tile_cs = self->ccs;
              gsk_gpu_cache_cache_tile (cache, texture, lod_level, scaling_filter, y * n_width + x, tile, tile_cs);
            }
          if (need_mipmap && !(gsk_gpu_image_get_flags (tile) & GSK_GPU_IMAGE_MIPMAP))
            gsk_gpu_mipmap_op (self->frame, tile);

          gsk_gpu_node_processor_image_op (self,
                                           tile,
                                           tile_cs,
                                           sampler,
                                           &tile_rect,
                                           &tile_rect);

          g_object_unref (tile);
        }
    }

out:
  g_clear_object (&memtex);
}

static GskGpuImage *
gsk_gpu_get_texture_tiles_as_image (GskGpuFrame            *frame,
                                    GdkColorState          *ccs,
                                    const graphene_rect_t  *clip_bounds,
                                    const graphene_vec2_t  *scale,
                                    const graphene_rect_t  *texture_bounds,
                                    GdkTexture             *texture,
                                    GskScalingFilter        scaling_filter)
{
  GskGpuRenderPass self;
  GskGpuImage *image;
  GskGpuRenderPassBlendStorage storage;

  image = gsk_gpu_node_processor_init_draw (&self,
                                            frame,
                                            ccs,
                                            gdk_texture_get_depth (texture),
                                            scale,
                                            clip_bounds);
  if (image == NULL)
    return NULL;

  gsk_gpu_render_pass_push_blend (&self, GSK_GPU_BLEND_ADD, &storage);

  gsk_gpu_node_processor_draw_texture_tiles (&self,
                                             texture_bounds,
                                             texture,
                                             scaling_filter);

  gsk_gpu_render_pass_pop_blend (&self, &storage);
  gsk_gpu_render_pass_finish (&self);

  return image;
}

static void
gsk_gpu_node_processor_add_texture_node (GskGpuRenderPass *self,
                                         GskRenderNode       *node)
{
  GdkColorState *image_cs;
  GskGpuImage *image;
  GdkTexture *texture;
  gboolean should_mipmap;
  GskGpuSampler sampler;

  texture = gsk_texture_node_get_texture (node);
  should_mipmap = texture_node_should_mipmap (node, self->frame, &self->scale);

  image = gsk_gpu_lookup_texture (self->frame, self->ccs, texture, should_mipmap, &image_cs);

  if (image == NULL)
    {
      graphene_rect_t clip, rounded_clip;

      if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip))
        return;

      if (!gsk_rect_snap_to_grid (&clip, &self->scale, &self->offset, &rounded_clip))
        return;

      image = gsk_gpu_get_texture_tiles_as_image (self->frame,
                                                  self->ccs,
                                                  &rounded_clip,
                                                  &self->scale,
                                                  &node->bounds,
                                                  texture,
                                                  should_mipmap ? GSK_SCALING_FILTER_TRILINEAR : GSK_SCALING_FILTER_LINEAR);
      gsk_gpu_node_processor_image_op (self,
                                       image,
                                       self->ccs,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &clip,
                                       &rounded_clip);
      g_object_unref (image);
      return;
    }

  if (should_mipmap)
    sampler = GSK_GPU_SAMPLER_MIPMAP_DEFAULT;
  else
    sampler = GSK_GPU_SAMPLER_DEFAULT;

  if (!gsk_gpu_image_supports_sampler (image, sampler) ||
      (should_mipmap && !gdk_color_state_equal (image_cs, self->ccs)))
    {
      image = gsk_gpu_copy_image (self->frame, self->ccs, image, image_cs, TRUE);
      gdk_color_state_unref (image_cs);
      image_cs = gdk_color_state_ref (self->ccs);
      gsk_gpu_cache_cache_texture_image (gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (self->frame)),
                                         texture,
                                         image,
                                         image_cs);
    }

  if (should_mipmap && !(gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_MIPMAP))
    gsk_gpu_mipmap_op (self->frame, image);

  gsk_gpu_node_processor_image_op (self,
                                   image,
                                   image_cs,
                                   sampler,
                                   &node->bounds,
                                   &node->bounds);

  gdk_color_state_unref (image_cs);
  g_object_unref (image);
}

static GskGpuImage *
gsk_gpu_get_texture_node_as_image (GskGpuFrame           *frame,
                                   GskGpuAsImageFlags     flags,
                                   GdkColorState         *ccs,
                                   const graphene_rect_t *clip_bounds,
                                   const graphene_vec2_t *scale,
                                   GskRenderNode         *node,
                                   graphene_rect_t       *out_bounds)
{
  GdkTexture *texture = gsk_texture_node_get_texture (node);
  GdkColorState *image_cs;
  GskGpuImage *image;
  gboolean should_mipmap;

  if ((flags & GSK_GPU_AS_IMAGE_EXACT_SIZE) &&
      !gsk_rect_equal (clip_bounds, &node->bounds))
    return gsk_gpu_get_node_as_image_via_offscreen (frame, flags, ccs, clip_bounds, scale, node, out_bounds);

  should_mipmap = texture_node_should_mipmap (node, frame, scale);
  image = gsk_gpu_lookup_texture (frame, ccs, texture, FALSE, &image_cs);

  if (image == NULL)
    {
      image = gsk_gpu_get_texture_tiles_as_image (frame,
                                                  ccs,
                                                  clip_bounds,
                                                  scale,
                                                  &node->bounds,
                                                  gsk_texture_node_get_texture (node),
                                                  should_mipmap ? GSK_SCALING_FILTER_TRILINEAR : GSK_SCALING_FILTER_LINEAR);
      *out_bounds = *clip_bounds;
      return image;
    }

  if (should_mipmap)
    {
      gdk_color_state_unref (image_cs);
      g_object_unref (image);
      return gsk_gpu_get_node_as_image_via_offscreen (frame, flags, ccs, clip_bounds, scale, node, out_bounds);
    }

  if (!gdk_color_state_equal (ccs, image_cs) ||
      gsk_gpu_image_get_shader_op (image) != GDK_SHADER_DEFAULT ||
      ((flags & GSK_GPU_AS_IMAGE_SAMPLED_OUT_OF_BOUNDS) &&
       gdk_memory_format_alpha (gsk_gpu_image_get_format (image)) == GDK_MEMORY_ALPHA_OPAQUE))
    {
      image = gsk_gpu_copy_image (frame, ccs, image, image_cs, FALSE);
      gdk_color_state_unref (image_cs);
      image_cs = gdk_color_state_ref (ccs);
      gsk_gpu_cache_cache_texture_image (gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (frame)),
                                         texture,
                                         image,
                                         ccs);
    }

  gdk_color_state_unref (image_cs);
  *out_bounds = node->bounds;
  return image;
}

static void
gsk_gpu_node_processor_add_texture_scale_node (GskGpuRenderPass *self,
                                               GskRenderNode       *node)
{
  GskGpuImage *image;
  GdkTexture *texture;
  GdkColorState *image_cs;
  GskScalingFilter scaling_filter;
  GskGpuSampler sampler;
  gboolean need_mipmap, need_offscreen;

  texture = gsk_texture_scale_node_get_texture (node);
  scaling_filter = gsk_texture_scale_node_get_filter (node);
  sampler = gsk_gpu_sampler_for_scaling_filter (scaling_filter),
  need_mipmap = scaling_filter == GSK_SCALING_FILTER_TRILINEAR;
  image = gsk_gpu_lookup_texture (self->frame, self->ccs, texture, need_mipmap, &image_cs);

  need_offscreen = image == NULL ||
                   self->modelview != NULL ||
                   !graphene_vec2_equal (&self->scale, graphene_vec2_one ());

  if (need_offscreen)
    {
      GskGpuImage *offscreen;
      graphene_rect_t clip_bounds;

      if (!gsk_gpu_render_pass_get_clip_bounds (self, &clip_bounds))
        return;

      /* first round to pixel boundaries, so we make sure the full pixels are covered */
      if (!gsk_rect_snap_to_grid (&clip_bounds, &self->scale, &self->offset, &clip_bounds))
        {
          if (image)
            {
              gdk_color_state_unref (image_cs);
              g_object_unref (image);
            }
          return;
        }
      /* then expand by half a pixel so that pixels needed for eventual linear
       * filtering are available */
      graphene_rect_inset (&clip_bounds, -0.5, -0.5);
      /* finally, round to full pixels */
      gsk_rect_round_larger (&clip_bounds);
      /* now intersect with actual node bounds */
      if (!gsk_rect_intersection (&clip_bounds, &node->bounds, &clip_bounds))
        {
          if (image)
            {
              gdk_color_state_unref (image_cs);
              g_object_unref (image);
            }
          return;
        }
      clip_bounds.size.width = ceilf (clip_bounds.size.width);
      clip_bounds.size.height = ceilf (clip_bounds.size.height);
      if (image == NULL)
        {
          offscreen = gsk_gpu_get_texture_tiles_as_image (self->frame,
                                                          self->ccs,
                                                          &clip_bounds,
                                                          graphene_vec2_one (),
                                                          &node->bounds,
                                                          texture,
                                                          scaling_filter);
        }
      else
        {
          offscreen = gsk_gpu_node_processor_create_offscreen (self->frame,
                                                               self->ccs,
                                                               graphene_vec2_one (),
                                                               &clip_bounds,
                                                               node);
          gdk_color_state_unref (image_cs);
          g_object_unref (image);
        }
      gsk_gpu_node_processor_image_op (self,
                                       offscreen,
                                       self->ccs,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &node->bounds,
                                       &clip_bounds);

      g_object_unref (offscreen);
      return;
    }

  if (!gsk_gpu_image_supports_sampler (image, sampler) ||
      (need_mipmap && !gdk_color_state_equal (image_cs, self->ccs)))
    {
      image = gsk_gpu_copy_image (self->frame, self->ccs, image, image_cs, need_mipmap);
      gdk_color_state_unref (image_cs);
      image_cs = gdk_color_state_ref (self->ccs);
      gsk_gpu_cache_cache_texture_image (gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (self->frame)),
                                         texture,
                                         image,
                                         image_cs);
    }

  if (need_mipmap && !(gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_MIPMAP))
    gsk_gpu_mipmap_op (self->frame, image);

  gsk_gpu_node_processor_image_op (self,
                                   image,
                                   image_cs,
                                   sampler,
                                   &node->bounds,
                                   &node->bounds);

  gdk_color_state_unref (image_cs);
  g_object_unref (image);
}

static GskGpuImage *
gsk_gpu_get_cairo_node_as_image (GskGpuFrame           *frame,
                                 GskGpuAsImageFlags     flags,
                                 GdkColorState         *ccs,
                                 const graphene_rect_t *clip_bounds,
                                 const graphene_vec2_t *scale,
                                 GskRenderNode         *node,
                                 graphene_rect_t       *out_bounds)
{
  GskGpuImage *result;

  if (!gdk_color_state_equal (ccs, GDK_COLOR_STATE_SRGB))
    return gsk_gpu_get_node_as_image_via_offscreen (frame, flags, ccs, clip_bounds, scale, node, out_bounds);

  result = gsk_gpu_upload_cairo_op (frame,
                                    scale,
                                    clip_bounds,
                                    (GskGpuCairoFunc) gsk_render_node_draw_fallback,
                                    gsk_render_node_ref (node),
                                    (GDestroyNotify) gsk_render_node_unref);

  g_object_ref (result);

  *out_bounds = *clip_bounds;
  return result;
}

static void
gsk_gpu_node_processor_add_inset_shadow_node (GskGpuRenderPass *self,
                                              GskRenderNode       *node)
{
  float spread, blur_radius;
  const GdkColor *color;
  const graphene_point_t *offset;

  color = gsk_inset_shadow_node_get_gdk_color (node);
  spread = gsk_inset_shadow_node_get_spread (node);
  blur_radius = gsk_inset_shadow_node_get_blur_radius (node);
  offset = gsk_inset_shadow_node_get_offset (node);

  if (blur_radius < 0.01)
    {
      graphene_vec4_t widths;
      graphene_vec2_t offset_vec;

      graphene_vec4_init (&widths, spread, spread, spread, spread);
      graphene_vec2_init (&offset_vec, offset->x, offset->y);

      gsk_gpu_border_op (self,
                         self->ccs,
                         gsk_gpu_color_states_find (self->ccs, color),
                         &node->bounds,
                         gsk_inset_shadow_node_get_outline (node),
                         color,
                         color,
                         color,
                         color,
                         &widths,
                         &offset_vec);
    }
  else
    {
      gsk_gpu_box_shadow_op (self,
                             self->ccs,
                             gsk_gpu_color_states_find (self->ccs, color),
                             &node->bounds,
                             TRUE,
                             gsk_inset_shadow_node_get_outline (node),
                             &GRAPHENE_SIZE_INIT (offset->x, offset->y),
                             spread,
                             blur_radius,
                             color);
    }
}

static void
gsk_gpu_node_processor_add_outset_shadow_node (GskGpuRenderPass *self,
                                               GskRenderNode       *node)
{
  float spread, blur_radius;
  const GdkColor *color;
  const graphene_point_t *offset;

  color = gsk_outset_shadow_node_get_gdk_color (node);
  spread = gsk_outset_shadow_node_get_spread (node);
  blur_radius = gsk_outset_shadow_node_get_blur_radius (node);
  offset = gsk_outset_shadow_node_get_offset (node);

  if (blur_radius < 0.01)
    {
      graphene_vec4_t widths;
      graphene_vec2_t offset_vec;
      GskRoundedRect outline;

      graphene_vec4_init (&widths, spread, spread, spread, spread);
      graphene_vec2_init (&offset_vec, - offset->x, - offset->y);
      gsk_rounded_rect_init_copy (&outline, gsk_outset_shadow_node_get_outline (node));
      gsk_rounded_rect_shrink (&outline, -spread, -spread, -spread, -spread);
      gsk_rect_init_offset (&outline.bounds, &outline.bounds, offset);

      gsk_gpu_border_op (self,
                         self->ccs,
                         gsk_gpu_color_states_find (self->ccs, color),
                         &node->bounds,
                         &outline,
                         color,
                         color,
                         color,
                         color,
                         &widths,
                         &offset_vec);
    }
  else
    {
      gsk_gpu_box_shadow_op (self,
                             self->ccs,
                             gsk_gpu_color_states_find (self->ccs, color),
                             &node->bounds,
                             FALSE,
                             gsk_outset_shadow_node_get_outline (node),
                             &GRAPHENE_SIZE_INIT (offset->x, offset->y),
                             spread,
                             blur_radius,
                             color);
    }
}

typedef void (* GradientOpFunc) (GskGpuRenderPass   *self,
                                 GdkColorState         *target,
                                 GskRenderNode         *node,
                                 const GskGradientStop *stops,
                                 gsize                  n_stops);

static void
gsk_gpu_node_processor_add_gradient_node (GskGpuRenderPass   *self,
                                          GskRenderNode         *node,
                                          GdkColorState         *ics,
                                          const GskGradientStop *stops,
                                          gsize                  n_stops,
                                          GradientOpFunc         func)
{
  GskGradientStop real_stops[7];
  GskGpuRenderPass other;
  graphene_rect_t bounds;
  gsize i, j;
  GskGpuImage *image;
  GskGpuRenderPassBlendStorage storage;

  if (n_stops < 8 && GDK_IS_DEFAULT_COLOR_STATE (ics))
    {
      func (self, self->ccs, node, stops, n_stops);
      return;
    }

  if (!gsk_gpu_node_processor_clip_node_bounds_and_snap_to_grid (self, node, &bounds))
    return;

  image = gsk_gpu_node_processor_init_draw (&other,
                                            self->frame,
                                            ics,
                                            gdk_memory_depth_merge (gdk_color_state_get_depth (self->ccs),
                                                                    gsk_render_node_get_preferred_depth (node)),
                                            &self->scale,
                                            &bounds);

  g_return_if_fail (image != NULL);

  gsk_gpu_render_pass_push_blend (&other, GSK_GPU_BLEND_ADD, &storage);

  for (i = 0; i < n_stops; /* happens inside the loop */)
    {
      if (i == 0)
        {
          real_stops[0].offset = stops[i].offset;
          real_stops[0].transition_hint = stops[i].transition_hint;
          gdk_color_init_copy (&real_stops[i].color, &stops[i].color);
          i++;
        }
      else
        {
          real_stops[0].offset = stops[i - 1].offset;
          real_stops[0].transition_hint = stops[i - 1].transition_hint;
          gdk_color_init_copy (&real_stops[0].color, &stops[i - 1].color);
          real_stops[0].color.alpha *= 0;
        }
      for (j = 1; j < 6 && i < n_stops; j++)
        {
          real_stops[j].offset = stops[i].offset;
          real_stops[j].transition_hint = stops[i].transition_hint;
          gdk_color_init_copy (&real_stops[j].color, &stops[i].color);
          i++;
        }
      if (i == n_stops - 1)
        {
          g_assert (j == 6);
          real_stops[j].offset = stops[i].offset;
          real_stops[j].transition_hint = stops[i].transition_hint;
          gdk_color_init_copy (&real_stops[j].color, &stops[i].color);
          j++;
          i++;
        }
      else if (i < n_stops)
        {
          real_stops[j].offset = stops[i].offset;
          real_stops[j].transition_hint = stops[i].transition_hint;
          gdk_color_init_copy (&real_stops[j].color, &stops[i].color);
          real_stops[j].color.alpha *= 0;
          j++;
        }

      func (&other, NULL, node, real_stops, j);
    }

  gsk_gpu_render_pass_pop_blend (&other, &storage);
  gsk_gpu_render_pass_finish (&other);

  gsk_gpu_node_processor_image_op (self,
                                   image,
                                   ics,
                                   GSK_GPU_SAMPLER_DEFAULT,
                                   &node->bounds,
                                   &bounds);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_linear_gradient_op (GskGpuRenderPass   *self,
                                           GdkColorState         *target,
                                           GskRenderNode         *node,
                                           const GskGradientStop *stops,
                                           gsize                  n_stops)
{
  const GskGradient *gradient;
  GdkColor colors[7];
  graphene_vec4_t offsets[2];
  graphene_vec4_t hints[2];

  gradient = gsk_gradient_node_get_gradient (node);

  gsk_gpu_color_stops_to_shader (stops,
                                 n_stops,
                                 gsk_gradient_get_interpolation (gradient),
                                 gsk_gradient_get_hue_interpolation (gradient),
                                 colors,
                                 offsets,
                                 hints);

  gsk_gpu_linear_gradient_op (self,
                              target,
                              gsk_gradient_get_interpolation (gradient),
                              &node->bounds,
                              gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_GRADIENTS),
                              gsk_gradient_get_premultiplied (gradient),
                              gsk_gradient_get_repeat (gradient),
                              gsk_linear_gradient_node_get_start (node),
                              gsk_linear_gradient_node_get_end (node),
                              &colors[0],
                              &colors[1],
                              &colors[2],
                              &colors[3],
                              &colors[4],
                              &colors[5],
                              &colors[6],
                              &offsets[0],
                              &offsets[1],
                              &hints[0],
                              &hints[1]);
}

static void
gsk_gpu_node_processor_add_linear_gradient_node (GskGpuRenderPass *self,
                                                 GskRenderNode       *node)
{
  const GskGradient *gradient = gsk_gradient_node_get_gradient (node);

  if (gsk_linear_gradient_node_is_zero_length (node))
    {
      switch (gsk_gradient_get_repeat (gradient))
        {
        case GSK_REPEAT_NONE:
          return;

        case GSK_REPEAT_PAD:
          /* average first and last color stop */
          {
            GdkColor color, start, end;
            GdkColorState *interpolation = gsk_gradient_get_interpolation (gradient);
            gdk_color_convert (&start,
                               interpolation,
                               gsk_gradient_get_stop_color (gradient, 0));
            gdk_color_convert (&end,
                               interpolation,
                               gsk_gradient_get_stop_color (gradient, gsk_gradient_get_n_stops (gradient) - 1));
            gdk_color_init (&color,
                            interpolation,
                            (float[4]) { 0.5 * (start.values[0] + end.values[0]),
                                         0.5 * (start.values[1] + end.values[1]),
                                         0.5 * (start.values[2] + end.values[2]),
                                         0.5 * (start.values[3] + end.values[3]) });
            gsk_gpu_color_op (self,
                              self->ccs,
                              gsk_gpu_color_states_find (self->ccs, &color),
                              &node->bounds,
                              &color);
          }
          break;

        case GSK_REPEAT_REPEAT:
        case GSK_REPEAT_REFLECT:
          {
            GdkColor color;
            gsk_gradient_get_average_color (gradient, &color);
            gsk_gpu_color_op (self,
                              self->ccs,
                              gsk_gpu_color_states_find (self->ccs, &color),
                              &node->bounds,
                              &color);
            gdk_color_finish (&color);
            return;
          }

        default:
          g_assert_not_reached ();
          return;
        }
    }

  gsk_gpu_node_processor_add_gradient_node (self,
                                            node,
                                            gsk_gradient_get_interpolation (gradient),
                                            gsk_gradient_get_stops (gradient),
                                            gsk_gradient_get_n_stops (gradient),
                                            gsk_gpu_node_processor_linear_gradient_op);
}

static void
gsk_gpu_node_processor_radial_gradient_op (GskGpuRenderPass   *self,
                                           GdkColorState         *target,
                                           GskRenderNode         *node,
                                           const GskGradientStop *stops,
                                           gsize                  n_stops)
{
  const graphene_point_t *start_center, *end_center;
  float start_radius, end_radius, aspect_ratio;
  const GskGradient *gradient;
  GdkColor colors[7];
  graphene_vec4_t offsets[2];
  graphene_vec4_t hints[2];

  gradient = gsk_gradient_node_get_gradient (node);
  start_center = gsk_radial_gradient_node_get_start_center (node);
  start_radius = gsk_radial_gradient_node_get_start_radius (node);
  end_center = gsk_radial_gradient_node_get_end_center (node);
  end_radius = gsk_radial_gradient_node_get_end_radius (node);
  aspect_ratio = gsk_radial_gradient_node_get_aspect_ratio (node);

  gsk_gpu_color_stops_to_shader (stops,
                                 n_stops,
                                 gsk_gradient_get_interpolation (gradient),
                                 gsk_gradient_get_hue_interpolation (gradient),
                                 colors,
                                 offsets,
                                 hints);

  gsk_gpu_radial_gradient_op (self,
                              target,
                              gsk_gradient_get_interpolation (gradient),
                              &node->bounds,
                              gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_GRADIENTS),
                              graphene_point_equal (start_center, end_center),
                              gsk_gradient_get_premultiplied (gradient),
                              gsk_gradient_get_repeat (gradient),
                              &colors[0],
                              &colors[1],
                              &colors[2],
                              &colors[3],
                              &colors[4],
                              &colors[5],
                              &colors[6],
                              &offsets[0],
                              &offsets[1],
                              &hints[0],
                              &hints[1],
                              start_center,
                              &GRAPHENE_SIZE_INIT (
                                  start_radius,
                                  start_radius / aspect_ratio
                              ),
                              end_center,
                              &GRAPHENE_SIZE_INIT (
                                  end_radius,
                                  end_radius / aspect_ratio
                              ));
}

static void
gsk_gpu_node_processor_add_radial_gradient_node (GskGpuRenderPass *self,
                                                 GskRenderNode       *node)
{
  const GskGradient *gradient = gsk_gradient_node_get_gradient (node);

  if (gsk_radial_gradient_node_is_zero_length (node))
    {
      switch (gsk_gradient_get_repeat (gradient))
        {
        case GSK_REPEAT_NONE:
          return;

        case GSK_REPEAT_PAD:
          /* The default rendering does the right thing */
          break;

        case GSK_REPEAT_REPEAT:
        case GSK_REPEAT_REFLECT:
          {
            GdkColor color;
            gsk_gradient_get_average_color (gradient, &color);
            gsk_gpu_color_op (self,
                              self->ccs,
                              gsk_gpu_color_states_find (self->ccs, &color),
                              &node->bounds,
                              &color);
            gdk_color_finish (&color);
            return;
          }

        default:
          g_assert_not_reached ();
          return;
        }
    }

  gsk_gpu_node_processor_add_gradient_node (self,
                                            node,
                                            gsk_gradient_get_interpolation (gradient),
                                            gsk_gradient_get_stops (gradient),
                                            gsk_gradient_get_n_stops (gradient),
                                            gsk_gpu_node_processor_radial_gradient_op);
}

static void
gsk_gpu_node_processor_conic_gradient_op (GskGpuRenderPass   *self,
                                          GdkColorState         *target,
                                          GskRenderNode         *node,
                                          const GskGradientStop *stops,
                                          gsize                  n_stops)
{
  const GskGradient *gradient;
  GdkColor colors[7];
  graphene_vec4_t offsets[2];
  graphene_vec4_t hints[2];

  gradient = gsk_gradient_node_get_gradient (node);

  gsk_gpu_color_stops_to_shader (stops,
                                 n_stops,
                                 gsk_gradient_get_interpolation (gradient),
                                 gsk_gradient_get_hue_interpolation (gradient),
                                 colors,
                                 offsets,
                                 hints);

  gsk_gpu_conic_gradient_op (self,
                             target,
                             gsk_gradient_get_interpolation (gradient),
                             &node->bounds,
                             gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_GRADIENTS),
                             gsk_gradient_get_premultiplied (gradient),
                             gsk_conic_gradient_node_get_center (node),
                             gsk_conic_gradient_node_get_angle (node),
                             &colors[0],
                             &colors[1],
                             &colors[2],
                             &colors[3],
                             &colors[4],
                             &colors[5],
                             &colors[6],
                             &offsets[0],
                             &offsets[1],
                             &hints[0],
                             &hints[1]);
}

static void
gsk_gpu_node_processor_add_conic_gradient_node (GskGpuRenderPass *self,
                                                GskRenderNode       *node)
{
  const GskGradient *gradient = gsk_gradient_node_get_gradient (node);
  gsk_gpu_node_processor_add_gradient_node (self,
                                            node,
                                            gsk_gradient_get_interpolation (gradient),
                                            gsk_gradient_get_stops (gradient),
                                            gsk_gradient_get_n_stops (gradient),
                                            gsk_gpu_node_processor_conic_gradient_op);
}

static void
gsk_gpu_node_processor_add_blur_node (GskGpuRenderPass *self,
                                      GskRenderNode       *node)
{
  GskRenderNode *child;
  GskGpuImage *image;
  graphene_rect_t tex_rect, clip_rect;
  float blur_radius, clip_radius;

  child = gsk_blur_node_get_child (node);
  blur_radius = gsk_blur_node_get_radius (node);
  if (blur_radius <= 0.f)
    {
      gsk_gpu_node_processor_add_node (self, child, 0);
      return;
    }

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius / 2.0);
  if (!gsk_gpu_render_pass_get_clip_bounds (self, &clip_rect))
    return;
  graphene_rect_inset (&clip_rect, -clip_radius, -clip_radius);
  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    GSK_GPU_AS_IMAGE_SAMPLED_OUT_OF_BOUNDS,
                                                    &clip_rect,
                                                    child,
                                                    0,
                                                    &tex_rect);
  if (image == NULL)
    return;

  gsk_gpu_node_processor_blur_op (self,
                                  &node->bounds,
                                  graphene_point_zero (),
                                  blur_radius,
                                  NULL,
                                  image,
                                  gdk_memory_format_get_depth (gsk_gpu_image_get_format (image),
                                                               gsk_gpu_image_get_conversion (image) == GSK_GPU_CONVERSION_SRGB),
                                  &tex_rect);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_shadow_node (GskGpuRenderPass *self,
                                        GskRenderNode       *node)
{
  GskGpuImage *image;
  graphene_rect_t clip_bounds, tex_rect;
  GskRenderNode *child;
  gsize i, n_shadows;

  if (self->opacity < 1.0)
    {
      gsk_gpu_node_processor_add_with_offscreen (self, node);
      return;
    }

  n_shadows = gsk_shadow_node_get_n_shadows (node);
  child = gsk_shadow_node_get_child (node);
  /* enlarge clip for shadow offsets */
  if (!gsk_gpu_render_pass_get_clip_bounds (self, &clip_bounds))
    return;
  clip_bounds = GRAPHENE_RECT_INIT (clip_bounds.origin.x - node->bounds.size.width + child->bounds.size.width - node->bounds.origin.x + child->bounds.origin.x,
                                    clip_bounds.origin.y - node->bounds.size.height + child->bounds.size.height - node->bounds.origin.y + child->bounds.origin.y,
                                    clip_bounds.size.width + node->bounds.size.width - child->bounds.size.width,
                                    clip_bounds.size.height + node->bounds.size.height - child->bounds.size.height);

  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    GSK_GPU_AS_IMAGE_SAMPLED_OUT_OF_BOUNDS,
                                                    &clip_bounds, 
                                                    child,
                                                    0,
                                                    &tex_rect);
  if (image == NULL)
    return;

  for (i = 0; i < n_shadows; i++)
    {
      const GskShadowEntry *shadow = gsk_shadow_node_get_shadow_entry (node, i);

      if (shadow->radius == 0)
        {
          GskGpuRenderPassTranslateStorage storage;

          gsk_gpu_render_pass_push_translate (self, &shadow->offset, &storage);
          gsk_gpu_colorize_op (self,
                               self->ccs,
                               gsk_gpu_color_states_find (self->ccs, &shadow->color),
                               &tex_rect,
                               image,
                               GSK_GPU_SAMPLER_TRANSPARENT,
                               &tex_rect,
                               &shadow->color);
          gsk_gpu_render_pass_pop_translate (self, &storage);
        }
      else
        {
          graphene_rect_t bounds;
          float clip_radius = gsk_cairo_blur_compute_pixels (0.5 * shadow->radius);
          graphene_rect_inset_r (&child->bounds, - clip_radius, - clip_radius, &bounds);
          gsk_gpu_node_processor_blur_op (self,
                                          &bounds,
                                          &shadow->offset,
                                          shadow->radius,
                                          &shadow->color,
                                          image,
                                          gdk_memory_format_get_depth (gsk_gpu_image_get_format (image),
                                                                       gsk_gpu_image_get_conversion (image) == GSK_GPU_CONVERSION_SRGB),
                                          &tex_rect);
        }
    }

  gsk_gpu_texture_op (self,
                      self->ccs,
                      &tex_rect,
                      image,
                      GSK_GPU_SAMPLER_DEFAULT,
                      &tex_rect);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_gl_shader_node (GskGpuRenderPass *self,
                                           GskRenderNode       *node)
{
  gsk_gpu_color_op (self,
                    self->ccs,
                    GDK_COLOR_STATE_SRGB,
                    &node->bounds,
                    &GDK_COLOR_SRGB (1, 105/255., 180/255., 1));
}

static void
gsk_gpu_node_processor_add_blend_node (GskGpuRenderPass *self,
                                       GskRenderNode       *node)
{
  GskRenderNode *bottom_child, *top_child;
  graphene_rect_t bottom_rect, top_rect;
  GskGpuImage *bottom_image, *top_image;

  bottom_child = gsk_blend_node_get_bottom_child (node);
  top_child = gsk_blend_node_get_top_child (node);

  bottom_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                           0,
                                                           NULL,
                                                           bottom_child,
                                                           0,
                                                           &bottom_rect);
  top_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                        0,
                                                        NULL,
                                                        top_child,
                                                        1,
                                                        &top_rect);

  if (bottom_image == NULL)
    {
      if (top_image == NULL)
        return;

      bottom_image = g_object_ref (top_image);
      bottom_rect = *graphene_rect_zero ();
    }
  else if (top_image == NULL)
    {
      top_image = g_object_ref (bottom_image);
      top_rect = *graphene_rect_zero ();
    }

  gsk_gpu_blend_mode_op (self,
                         self->ccs,
                         gsk_blend_node_get_color_state (node),
                         &node->bounds,
                         bottom_image,
                         GSK_GPU_SAMPLER_DEFAULT,
                         top_image,
                         GSK_GPU_SAMPLER_DEFAULT,
                         gsk_blend_node_get_blend_mode (node),
                         &node->bounds,
                         &bottom_rect,
                         &top_rect);

  g_object_unref (top_image);
  g_object_unref (bottom_image);
}

static void
gsk_gpu_node_processor_add_arithmetic_node (GskGpuRenderPass *self,
                                            GskRenderNode       *node)
{
  float k1, k2, k3, k4;
  GskRenderNode *first_child, *second_child;
  graphene_rect_t first_rect, second_rect;
  GskGpuImage *first_image, *second_image;

  gsk_arithmetic_node_get_factors (node, &k1, &k2, &k3, &k4);

  first_child = gsk_arithmetic_node_get_first_child (node);
  second_child = gsk_arithmetic_node_get_second_child (node);

  first_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                          0,
                                                          NULL,
                                                          first_child,
                                                          0,
                                                          &first_rect);
  second_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                           0,
                                                           NULL,
                                                           second_child,
                                                           1,
                                                           &second_rect);

  if (first_image == NULL)
    {
      if (second_image == NULL)
        return;

      first_image = g_object_ref (second_image);
      first_rect = *graphene_rect_zero ();
    }
  else if (second_image == NULL)
    {
      second_image = g_object_ref (first_image);
      second_rect = *graphene_rect_zero ();
    }

  gsk_gpu_arithmetic_op (self,
                         self->ccs,
                         gsk_arithmetic_node_get_color_state (node),
                         &node->bounds,
                         first_image,
                         GSK_GPU_SAMPLER_DEFAULT,
                         second_image,
                         GSK_GPU_SAMPLER_DEFAULT,
                         &node->bounds,
                         &first_rect,
                         &second_rect,
                         k1, k2, k3, k4);

  g_object_unref (first_image);
  g_object_unref (second_image);
}

static void
gsk_gpu_node_processor_add_cross_fade_node (GskGpuRenderPass *self,
                                            GskRenderNode       *node)
{
  GskRenderNode *start_child, *end_child;
  graphene_rect_t start_rect, end_rect;
  GskGpuImage *start_image, *end_image;
  float progress, old_opacity;

  start_child = gsk_cross_fade_node_get_start_child (node);
  end_child = gsk_cross_fade_node_get_end_child (node);
  progress = gsk_cross_fade_node_get_progress (node);

  if (progress <= 0.0)
    {
      gsk_gpu_node_processor_add_node (self, start_child, 0);
      return;
    }
  if (progress >= 1.0)
    {
      gsk_gpu_node_processor_add_node (self, end_child, 1);
      return;
    }

  start_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                          0,
                                                          NULL,
                                                          start_child,
                                                          0,
                                                          &start_rect);
  end_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                        0,
                                                        NULL,
                                                        end_child,
                                                        1,
                                                        &end_rect);

  if (start_image == NULL)
    {
      if (end_image == NULL)
        return;

      old_opacity = self->opacity;
      self->opacity *= progress;
      gsk_gpu_node_processor_image_op (self,
                                       end_image,
                                       self->ccs,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &end_child->bounds,
                                       &end_rect);
      g_object_unref (end_image);
      self->opacity = old_opacity;
      return;
    }
  else if (end_image == NULL)
    {
      old_opacity = self->opacity;
      self->opacity *= (1 - progress);
      gsk_gpu_node_processor_image_op (self,
                                       start_image,
                                       self->ccs,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &start_child->bounds,
                                       &start_rect);
      g_object_unref (start_image);
      self->opacity = old_opacity;
      return;
    }

  gsk_gpu_cross_fade_op (self,
                         self->ccs,
                         &node->bounds,
                         start_image,
                         GSK_GPU_SAMPLER_DEFAULT,
                         end_image,
                         GSK_GPU_SAMPLER_DEFAULT,
                         &node->bounds,
                         &start_rect,
                         &end_rect,
                         progress);

  g_object_unref (end_image);
  g_object_unref (start_image);
}

static void
gsk_gpu_node_processor_add_displacement_node (GskGpuRenderPass *self,
                                              GskRenderNode       *node)
{
  graphene_rect_t bounds, child_bounds, displacement_rect, child_rect;
  GskRenderNode *displacement_child, *child;
  GskGpuImage *displacement_image, *child_image;
  const graphene_size_t *max;
  const GdkColorChannel *channels;
  const graphene_point_t *offset;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds))
    return;

  displacement_child = gsk_displacement_node_get_displacement (node);
  child = gsk_displacement_node_get_child (node);
  max = gsk_displacement_node_get_max (node);
  channels = gsk_displacement_node_get_channels (node);
  offset = gsk_displacement_node_get_offset (node);

  child_bounds = bounds;
  graphene_rect_inset (&child_bounds, - max->width, - max->height);
  child_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                          0,
                                                          &child_bounds,
                                                          child,
                                                          0,
                                                          &child_rect);
  if (child_image == NULL)
    return;

  displacement_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                                 0,
                                                                 &bounds,
                                                                 displacement_child,
                                                                 1,
                                                                 &displacement_rect);
  if (displacement_image == NULL)
    return; /* technically we have to render TRANSPARENT everywhere */

  gsk_gpu_displacement_op (self,
                           self->ccs,
                           &bounds,
                           displacement_image,
                           GSK_GPU_SAMPLER_TRANSPARENT,
                           child_image,
                           GSK_GPU_SAMPLER_TRANSPARENT,
                           &bounds,
                           &displacement_rect,
                           &child_rect,
                           channels[0],
                           channels[1],
                           max,
                           gsk_displacement_node_get_scale (node),
                           &GRAPHENE_SIZE_INIT (offset->x, offset->y));

  g_object_unref (displacement_image);
  g_object_unref (child_image);
}

static void
gsk_gpu_node_processor_add_mask_node (GskGpuRenderPass *self,
                                      GskRenderNode       *node)
{
  GskRenderNode *source_child, *mask_child;
  GskGpuImage *mask_image;
  graphene_rect_t bounds, mask_rect;
  GskMaskMode mask_mode;

  source_child = gsk_mask_node_get_source (node);
  mask_child = gsk_mask_node_get_mask (node);
  mask_mode = gsk_mask_node_get_mask_mode (node);

  if (!gsk_gpu_node_processor_clip_node_bounds_and_snap_to_grid (self, node, &bounds))
    return;

  mask_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                         0,
                                                         &bounds,
                                                         mask_child,
                                                         1,
                                                         &mask_rect);
  if (mask_image == NULL)
    {
      if (mask_mode == GSK_MASK_MODE_INVERTED_ALPHA)
        gsk_gpu_node_processor_add_node (self, source_child, 0);
      return;
    }

  if (gsk_render_node_get_node_type (source_child) == GSK_COLOR_NODE &&
      mask_mode == GSK_MASK_MODE_ALPHA)
    {
      const GdkColor *color = gsk_color_node_get_gdk_color (source_child);
      gsk_gpu_colorize_op (self,
                           self->ccs,
                           gsk_gpu_color_states_find (self->ccs, color),
                           &bounds,
                           mask_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           &mask_rect,
                           color);
    }
  else
    {
      GskGpuImage *source_image;
      graphene_rect_t source_rect;

      source_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                               0,
                                                               &bounds,
                                                               source_child,
                                                               0,
                                                               &source_rect);
      if (source_image == NULL)
        {
          g_object_unref (mask_image);
          return;
        }

      gsk_gpu_mask_op (self,
                       self->ccs,
                       &bounds,
                       source_image,
                       GSK_GPU_SAMPLER_DEFAULT,
                       mask_image,
                       GSK_GPU_SAMPLER_DEFAULT,
                       mask_mode,
                       &source_rect,
                       &mask_rect);

      g_object_unref (source_image);
    }

  g_object_unref (mask_image);
}

static void
gsk_gpu_node_processor_add_glyph_node (GskGpuRenderPass *self,
                                       GskRenderNode       *node)
{
  GskGpuCache *cache;
  const PangoGlyphInfo *glyphs;
  PangoFont *font;
  graphene_point_t offset;
  guint num_glyphs;
  float scale;
  float align_scale_x, align_scale_y;
  unsigned int flags_mask;
  const float pango_scale = PANGO_SCALE;
  cairo_hint_style_t hint_style;
  const GdkColor *color;
  GdkColorState *acs;
  GdkColor color2;

  if (self->opacity < 1.0 &&
      gsk_text_node_has_color_glyphs (node))
    {
      gsk_gpu_node_processor_add_with_offscreen (self, node);
      return;
    }

  cache = gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (self->frame));

  glyphs = gsk_text_node_get_glyphs (node, &num_glyphs);
  font = gsk_text_node_get_font (node);
  offset = *gsk_text_node_get_offset (node);
  hint_style = gsk_text_node_get_font_hint_style (node);
  color = gsk_text_node_get_gdk_color (node);

  acs = gsk_gpu_color_states_find (self->ccs, color);
  gdk_color_convert (&color2, acs, color);

  scale = MAX (graphene_vec2_get_x (&self->scale), graphene_vec2_get_y (&self->scale));

  if (hint_style != CAIRO_HINT_STYLE_NONE)
    {
      align_scale_x = scale * 4;
      align_scale_y = scale;
      flags_mask = 3;
    }
  else
    {
      align_scale_x = align_scale_y = scale * 4;
      flags_mask = 15;
    }

  for (guint i = 0; i < num_glyphs; i++)
    {
      GskGpuImage *image;
      graphene_rect_t glyph_bounds, glyph_tex_rect;
      graphene_point_t glyph_offset, glyph_origin;
      GskGpuGlyphLookupFlags flags;

      glyph_origin = GRAPHENE_POINT_INIT (offset.x + glyphs[i].geometry.x_offset / pango_scale,
                                          offset.y + glyphs[i].geometry.y_offset / pango_scale);

      glyph_origin.x = floorf (glyph_origin.x * align_scale_x + 0.5f);
      glyph_origin.y = floorf (glyph_origin.y * align_scale_y + 0.5f);
      flags = (((int) glyph_origin.x & 3) | (((int) glyph_origin.y & 3) << 2)) & flags_mask;
      glyph_origin.x /= align_scale_x;
      glyph_origin.y /= align_scale_y;

      image = gsk_gpu_cached_glyph_lookup (cache,
                                           self->frame,
                                           font,
                                           glyphs[i].glyph,
                                           flags,
                                           scale,
                                           &glyph_bounds,
                                           &glyph_offset);

      glyph_origin.x -= glyph_offset.x / scale;
      glyph_origin.y -= glyph_offset.y / scale;
      glyph_tex_rect = GRAPHENE_RECT_INIT (glyph_origin.x - glyph_bounds.origin.x / scale,
                                           glyph_origin.y - glyph_bounds.origin.y / scale,
                                           gsk_gpu_image_get_width (image) / scale,
                                           gsk_gpu_image_get_height (image) / scale);
      glyph_bounds = GRAPHENE_RECT_INIT (glyph_origin.x,
                                         glyph_origin.y,
                                         glyph_bounds.size.width / scale,
                                         glyph_bounds.size.height / scale);

      if (glyphs[i].attr.is_color)
        gsk_gpu_texture_op (self,
                            self->ccs,
                            &glyph_bounds,
                            image,
                            GSK_GPU_SAMPLER_DEFAULT,
                            &glyph_tex_rect);
      else
        gsk_gpu_colorize_op (self,
                             self->ccs,
                             acs,
                             &glyph_bounds,
                             image,
                             GSK_GPU_SAMPLER_DEFAULT,
                             &glyph_tex_rect,
                             &color2);

      offset.x += glyphs[i].geometry.width / pango_scale;
    }

  gdk_color_finish (&color2);
}

static void
gsk_gpu_node_processor_add_color_matrix_node (GskGpuRenderPass *self,
                                              GskRenderNode       *node)
{
  GskGpuImage *image;
  GskRenderNode *child;
  graphene_matrix_t opacity_matrix;
  const graphene_matrix_t *color_matrix;
  graphene_rect_t tex_rect;

  child = gsk_color_matrix_node_get_child (node);

  color_matrix = gsk_color_matrix_node_get_color_matrix (node);
  if (self->opacity < 1.0f)
    {
      graphene_matrix_init_from_float (&opacity_matrix,
                                       (float[16]) {
                                           1.0f, 0.0f, 0.0f, 0.0f,
                                           0.0f, 1.0f, 0.0f, 0.0f,
                                           0.0f, 0.0f, 1.0f, 0.0f,
                                           0.0f, 0.0f, 0.0f, self->opacity
                                      });
      graphene_matrix_multiply (&opacity_matrix, color_matrix, &opacity_matrix);
      color_matrix = &opacity_matrix;
    }

  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    0,
                                                    NULL,
                                                    child,
                                                    0,
                                                    &tex_rect);
  if (image == NULL)
    return;

  gsk_gpu_color_matrix_op (self,
                           self->ccs,
                           gsk_color_matrix_node_get_color_state (node),
                           &node->bounds,
                           image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           color_matrix,
                           gsk_color_matrix_node_get_color_offset (node),
                           &node->bounds,
                           &tex_rect);

  g_object_unref (image);
}

static void
copy_component_transfer (const GskComponentTransfer *transfer,
                         float                       params[4],
                         float                       table[32],
                         guint                      *n)
{
  params[0] = transfer->kind;
  switch (transfer->kind)
    {
    case GSK_COMPONENT_TRANSFER_IDENTITY:
      break;
    case GSK_COMPONENT_TRANSFER_LEVELS:
      params[1] = transfer->levels.n;
      break;
    case GSK_COMPONENT_TRANSFER_LINEAR:
      params[1] = transfer->linear.m;
      params[2] = transfer->linear.b;
      break;
    case GSK_COMPONENT_TRANSFER_GAMMA:
      params[1] = transfer->gamma.amp;
      params[2] = transfer->gamma.exp;
      params[3] = transfer->gamma.ofs;
      break;
    case GSK_COMPONENT_TRANSFER_DISCRETE:
    case GSK_COMPONENT_TRANSFER_TABLE:
      if (*n + transfer->table.n >= 32)
        g_warning ("tables too big in component transfer");
      params[1] = transfer->table.n;
      params[2] = *n;
      for (guint i = 0; i < transfer->table.n && *n + i < 32; i++)
        table[*n + i] = transfer->table.values[i];
      *n += transfer->table.n;
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
gsk_gpu_node_processor_add_component_transfer_node (GskGpuRenderPass *self,
                                                    GskRenderNode       *node)
{
  GskGpuImage *image;
  GskRenderNode *child;
  graphene_rect_t tex_rect;
  float params[4];
  graphene_vec4_t params_vec[4];
  float table[32];
  graphene_vec4_t table_vec[8];
  guint i, n;

  child = gsk_component_transfer_node_get_child (node);

  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    0,
                                                    NULL,
                                                    child,
                                                    0,
                                                    &tex_rect);
  if (image == NULL)
    return;

  n = 0;
  for (i = 0; i < 4; i++)
    {
      copy_component_transfer (gsk_component_transfer_node_get_transfer (node, i),
                               params,
                               table,
                               &n);
      graphene_vec4_init_from_float (&params_vec[i], params);
    }
  for (i = 0; i < 8; i++)
    {
      graphene_vec4_init_from_float (&table_vec[i], &table[4 * i]);
    }

  gsk_gpu_component_transfer_op (self,
                                 self->ccs,
                                 gsk_component_transfer_node_get_color_state (node),
                                 &node->bounds,
                                 image,
                                 GSK_GPU_SAMPLER_DEFAULT,
                                 &params_vec[0],
                                 &params_vec[1],
                                 &params_vec[2],
                                 &params_vec[3],
                                 &table_vec[0],
                                 &table_vec[1],
                                 &table_vec[2],
                                 &table_vec[3],
                                 &table_vec[4],
                                 &table_vec[5],
                                 &table_vec[6],
                                 &table_vec[7],
                                 &node->bounds,
                                 &tex_rect);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_repeat_tile (GskGpuRenderPass    *self,
                                    const graphene_rect_t  *rect,
                                    float                   x,
                                    float                   y,
                                    GskRenderNode          *child,
                                    const graphene_rect_t  *child_bounds)
{
  GskGpuImage *image;
  graphene_rect_t clipped_child_bounds, offset_rect;

  gsk_rect_init_offset (&offset_rect,
                        rect,
                        &GRAPHENE_POINT_INIT (- x * child_bounds->size.width,
                                              - y * child_bounds->size.height));
  if (!gsk_rect_intersection (&offset_rect, child_bounds, &clipped_child_bounds))
    {
      /* rounding error hits again */
      return;
    }

  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    GSK_GPU_AS_IMAGE_EXACT_SIZE,
                                                    &clipped_child_bounds,
                                                    child,
                                                    0,
                                                    &clipped_child_bounds);
  g_return_if_fail (image);

  gsk_gpu_texture_op (self,
                      self->ccs,
                      rect,
                      image,
                      GSK_GPU_SAMPLER_REPEAT,
                      &GRAPHENE_RECT_INIT (
                          clipped_child_bounds.origin.x + x * child_bounds->size.width,
                          clipped_child_bounds.origin.y + y * child_bounds->size.height,
                          clipped_child_bounds.size.width,
                          clipped_child_bounds.size.height
                      ));

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_repeat_node (GskGpuRenderPass *self,
                                        GskRenderNode       *node)
{
  GskRenderNode *child;
  const graphene_rect_t *child_bounds;
  graphene_rect_t bounds;
  float tile_left, tile_right, tile_top, tile_bottom;
  GskRepeat repeat;
  gboolean avoid_offscreen;

  child = gsk_repeat_node_get_child (node);
  child_bounds = gsk_repeat_node_get_child_bounds (node);
  if (gsk_rect_is_empty (child_bounds))
    return;

  repeat = gsk_repeat_node_get_repeat (node);
  if (repeat == GSK_REPEAT_NONE)
    {
      gsk_gpu_node_processor_add_node_clipped (self,
                                               child,
                                               0,
                                               &node->bounds);
      return;
    }

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds))
    return;

  tile_left = (bounds.origin.x - child_bounds->origin.x) / child_bounds->size.width;
  tile_right = (bounds.origin.x + bounds.size.width - child_bounds->origin.x) / child_bounds->size.width;
  tile_top = (bounds.origin.y - child_bounds->origin.y) / child_bounds->size.height;
  tile_bottom = (bounds.origin.y + bounds.size.height - child_bounds->origin.y) / child_bounds->size.height;
  avoid_offscreen = !gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_REPEAT);

  if (repeat == GSK_REPEAT_PAD)
    {
      graphene_rect_t clipped_child_bounds;
      GskGpuImage *image;

      gsk_repeat_node_compute_rect_for_pad (&bounds,
                                            child_bounds,
                                            &clipped_child_bounds);
      image = gsk_gpu_node_processor_get_node_as_image (self,
                                                        GSK_GPU_AS_IMAGE_EXACT_SIZE,
                                                        &clipped_child_bounds,
                                                        child,
                                                        0,
                                                        &clipped_child_bounds);
      g_return_if_fail (image);
      gsk_gpu_texture_op (self,
                          self->ccs,
                          &bounds,
                          image,
                          GSK_GPU_SAMPLER_DEFAULT,
                          &clipped_child_bounds);
      g_object_unref (image);
    }
  else if (repeat == GSK_REPEAT_REFLECT)
    {
      graphene_rect_t clipped_child_bounds;
      graphene_point_t pos;
      GskGpuImage *image;

      gsk_repeat_node_compute_rect_for_reflect (&bounds,
                                                child_bounds,
                                                &clipped_child_bounds,
                                                &pos);
      image = gsk_gpu_node_processor_get_node_as_image (self,
                                                        GSK_GPU_AS_IMAGE_EXACT_SIZE,
                                                        &clipped_child_bounds,
                                                        child,
                                                        0,
                                                        &clipped_child_bounds);
      g_return_if_fail (image);
      clipped_child_bounds.origin = pos;
      gsk_gpu_texture_op (self,
                          self->ccs,
                          &bounds,
                          image,
                          GSK_GPU_SAMPLER_REFLECT,
                          &clipped_child_bounds);
      g_object_unref (image);
    }
  else
    {
      /* the 1st check tests that a tile fully fits into the bounds,
       * the 2nd check is to catch the case where it fits exactly */
      if (!avoid_offscreen &&
          ceilf (tile_left) < floorf (tile_right) &&
          bounds.size.width > child_bounds->size.width)
        {
          if (ceilf (tile_top) < floorf (tile_bottom) &&
              bounds.size.height > child_bounds->size.height)
            {
              /* tile in both directions */
              gsk_gpu_node_processor_repeat_tile (self,
                                                  &bounds,
                                                  ceilf (tile_left),
                                                  ceilf (tile_top),
                                                  child,
                                                  child_bounds);
            }
          else
            {
              /* tile horizontally, repeat vertically */
              float y;
              for (y = floorf (tile_top); y < ceilf (tile_bottom); y++)
                {
                  float start_y = MAX (bounds.origin.y,
                                       child_bounds->origin.y + y * child_bounds->size.height);
                  float end_y = MIN (bounds.origin.y + bounds.size.height,
                                     child_bounds->origin.y + (y + 1) * child_bounds->size.height);
                  gsk_gpu_node_processor_repeat_tile (self,
                                                      &GRAPHENE_RECT_INIT (
                                                        bounds.origin.x,
                                                        start_y,
                                                        bounds.size.width,
                                                        end_y - start_y
                                                      ),
                                                      ceilf (tile_left),
                                                      y,
                                                      child,
                                                      child_bounds);
                }
            }
        }
      else if (!avoid_offscreen &&
               ceilf (tile_top) < floorf (tile_bottom) &&
               bounds.size.height > child_bounds->size.height)
        {
          /* repeat horizontally, tile vertically */
          float x;
          for (x = floorf (tile_left); x < ceilf (tile_right); x++)
            {
              float start_x = MAX (bounds.origin.x,
                                   child_bounds->origin.x + x * child_bounds->size.width);
              float end_x = MIN (bounds.origin.x + bounds.size.width,
                                 child_bounds->origin.x + (x + 1) * child_bounds->size.width);
              gsk_gpu_node_processor_repeat_tile (self,
                                                  &GRAPHENE_RECT_INIT (
                                                    start_x,
                                                    bounds.origin.y,
                                                    end_x - start_x,
                                                    bounds.size.height
                                                  ),
                                                  x,
                                                  ceilf (tile_top),
                                                  child,
                                                  child_bounds);
            }
        }
      else
        {
          /* repeat in both directions */
          graphene_point_t old_offset, offset;
          graphene_rect_t clip_bounds;
          float x, y;

          old_offset = self->offset;

          for (x = floorf (tile_left); x < ceilf (tile_right); x++)
            {
              offset.x = x * child_bounds->size.width;
              for (y = floorf (tile_top); y < ceilf (tile_bottom); y++)
                {
                  offset.y = y * child_bounds->size.height;
                  self->offset = GRAPHENE_POINT_INIT (old_offset.x + offset.x, old_offset.y + offset.y);
                  clip_bounds = GRAPHENE_RECT_INIT (bounds.origin.x - offset.x,
                                                    bounds.origin.y - offset.y,
                                                    bounds.size.width,
                                                    bounds.size.height);
                  if (!gsk_rect_intersection (&clip_bounds, child_bounds, &clip_bounds))
                    continue;
                  gsk_gpu_node_processor_add_node_clipped (self,
                                                           child,
                                                           0,
                                                           &clip_bounds);
                }
            }

          self->offset = old_offset;
        }
    }
}

static void
gsk_gpu_node_processor_add_fill_node (GskGpuRenderPass *self,
                                      GskRenderNode       *node)
{
  graphene_rect_t clip_bounds;
  GskGpuImage *mask_image;
  GskRenderNode *child;
  graphene_rect_t tex_rect;
  GskGpuCache *cache;

  if (!gsk_gpu_node_processor_clip_node_bounds_and_snap_to_grid (self, node, &clip_bounds))
    return;

  child = gsk_fill_node_get_child (node);

  cache = gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (self->frame));

  mask_image = gsk_gpu_cached_fill_lookup (cache,
                                           self->frame,
                                           &self->scale,
                                           &clip_bounds,
                                           gsk_fill_node_get_path (node),
                                           gsk_fill_node_get_fill_rule (node),
                                           &tex_rect);
  if (mask_image == NULL)
    return;

  if (GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE)
    {
      const GdkColor *color = gsk_color_node_get_gdk_color (child);

      gsk_gpu_colorize_op (self,
                           self->ccs,
                           gsk_gpu_color_states_find (self->ccs, color),
                           &clip_bounds,
                           mask_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           &tex_rect,
                           color);
    }
  else
    {
      GskGpuImage *source_image;
      graphene_rect_t source_rect;

      source_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                               0,
                                                               &clip_bounds,
                                                               child,
                                                               0,
                                                               &source_rect);

      if (source_image != NULL)
        {
          gsk_gpu_mask_op (self,
                           self->ccs,
                           &clip_bounds,
                           source_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           mask_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           GSK_MASK_MODE_ALPHA,
                           &source_rect,
                           &tex_rect);

          g_object_unref (source_image);
        }
    }

  g_object_unref (mask_image);
}

static void
gsk_gpu_node_processor_add_stroke_node (GskGpuRenderPass *self,
                                        GskRenderNode       *node)
{
  graphene_rect_t clip_bounds;
  GskGpuImage *mask_image;
  GskRenderNode *child;
  graphene_rect_t tex_rect;
  GskGpuCache *cache;

  if (!gsk_gpu_node_processor_clip_node_bounds_and_snap_to_grid (self, node, &clip_bounds))
    return;

  child = gsk_stroke_node_get_child (node);

  cache = gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (self->frame));

  mask_image = gsk_gpu_cached_stroke_lookup (cache,
                                             self->frame,
                                             &self->scale,
                                             &clip_bounds,
                                             gsk_stroke_node_get_path (node),
                                             gsk_stroke_node_get_stroke (node),
                                             &tex_rect);
  if (mask_image == NULL)
    return;

  if (GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE)
    {
      const GdkColor *color = gsk_color_node_get_gdk_color (child);

      gsk_gpu_colorize_op (self,
                           self->ccs,
                           gsk_gpu_color_states_find (self->ccs, color),
                           &clip_bounds,
                           mask_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           &tex_rect,
                           color);
    }
  else
    {
      GskGpuImage *source_image;
      graphene_rect_t source_rect;

      source_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                               0,
                                                               &clip_bounds,
                                                               child,
                                                               0,
                                                               &source_rect);
      if (source_image != NULL)
        {
          gsk_gpu_mask_op (self,
                           self->ccs,
                           &clip_bounds,
                           source_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           mask_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           GSK_MASK_MODE_ALPHA,
                           &source_rect,
                           &tex_rect);

          g_object_unref (source_image);
        }
    }

  g_object_unref (mask_image);
}

static void
gsk_gpu_node_processor_add_subsurface_node (GskGpuRenderPass *self,
                                            GskRenderNode       *node)
{
  GdkSubsurface *subsurface;

  subsurface = gsk_subsurface_node_get_subsurface (node);
  if (subsurface == NULL ||
      gdk_subsurface_get_texture (subsurface) == NULL ||
      gdk_subsurface_get_parent (subsurface) != gdk_draw_context_get_surface (gsk_gpu_frame_get_context (self->frame)))
    {
      gsk_gpu_node_processor_add_node (self, gsk_subsurface_node_get_child (node), 0);
      return;
    }

  if (!gdk_subsurface_is_above_parent (subsurface))
    {
      cairo_rectangle_int_t device_clipped;
      graphene_rect_t clipped;

      if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clipped))
        return;

      if (gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_CLEAR) &&
          node->bounds.size.width * node->bounds.size.height > 100 * 100 && /* not worth the effort for small images */
          (self->clip.type != GSK_GPU_CLIP_ROUNDED ||
           gsk_gpu_clip_contains_rect (&self->clip, &self->offset, &clipped)) &&
          gsk_gpu_render_pass_user_to_device_exact (self, &clipped, &device_clipped))
        {
          float color[4] = { 0, 0, 0, 0 };
          gsk_gpu_clear_op (self->frame, &device_clipped, color);
        }
      else
        {
          GskGpuRenderPassBlendStorage storage;
          GdkColor white;

          gsk_gpu_render_pass_push_blend (self, GSK_GPU_BLEND_CLEAR, &storage);

          gdk_color_init (&white, self->ccs, ((float[]) { 1, 1, 1, 1 }));
          gsk_gpu_color_op (self,
                            self->ccs,
                            self->ccs,
                            &node->bounds,
                            &white);
          gdk_color_finish (&white);

          gsk_gpu_render_pass_pop_blend (self, &storage);
        }
    }
}

static GskGpuImage *
gsk_gpu_get_subsurface_node_as_image (GskGpuFrame           *frame,
                                      GskGpuAsImageFlags     flags,
                                      GdkColorState         *ccs,
                                      const graphene_rect_t *clip_bounds,
                                      const graphene_vec2_t *scale,
                                      GskRenderNode         *node,
                                      graphene_rect_t       *out_bounds)
{
  GskGpuImage *result;
  GskRenderNode *child;
#ifndef G_DISABLE_ASSERT
  GdkSubsurface *subsurface;

  subsurface = gsk_subsurface_node_get_subsurface (node);
  g_assert (subsurface == NULL ||
            gdk_subsurface_get_texture (subsurface) == NULL ||
            gdk_subsurface_get_parent (subsurface) != gdk_draw_context_get_surface (gsk_gpu_frame_get_context (frame)));
#endif

  child = gsk_subsurface_node_get_child (node);

  gsk_gpu_frame_start_node (frame, child, 0);

  result = gsk_gpu_get_node_as_image (frame,
                                      flags,
                                      ccs,
                                      clip_bounds,
                                      scale,
                                      child,
                                      out_bounds);

  gsk_gpu_frame_end_node (frame);

  return result;
}

static void
gsk_gpu_node_processor_add_copy_node (GskGpuRenderPass *self,
                                      GskRenderNode       *node)
{
  g_warning_once ("Bug: The GPU renderer should never see copy nodes");
}

static void
gsk_gpu_node_processor_add_paste_node (GskGpuRenderPass *self,
                                       GskRenderNode       *node)
{
  g_warning_once ("Bug: The GPU renderer should never see paste nodes");
}

static gboolean
gsk_gpu_porter_duff_needs_dual_blend (GskPorterDuff op)
{
  switch (op)
  {
    case GSK_PORTER_DUFF_DEST:
    case GSK_PORTER_DUFF_SOURCE_OVER_DEST:
    case GSK_PORTER_DUFF_DEST_IN_SOURCE:
    case GSK_PORTER_DUFF_DEST_OUT_SOURCE:
    case GSK_PORTER_DUFF_CLEAR:
      return FALSE;

    case GSK_PORTER_DUFF_SOURCE:
    case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
    case GSK_PORTER_DUFF_SOURCE_IN_DEST:
    case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
    case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
    case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:
    case GSK_PORTER_DUFF_XOR:
      return TRUE;

    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

static GskGpuBlend
gsk_gpu_blend_for_porter_duff (GskPorterDuff op)
{
  switch (op)
    {
    case GSK_PORTER_DUFF_SOURCE:
      return GSK_GPU_BLEND_MASK_ONE;

    case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
    case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
    case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:
    case GSK_PORTER_DUFF_XOR:
      return GSK_GPU_BLEND_MASK_INV_ALPHA;

    case GSK_PORTER_DUFF_SOURCE_IN_DEST:
    case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
      return GSK_GPU_BLEND_MASK_ALPHA;

    case GSK_PORTER_DUFF_CLEAR:
    case GSK_PORTER_DUFF_DEST_IN_SOURCE:
    case GSK_PORTER_DUFF_DEST_OUT_SOURCE:
      return GSK_GPU_BLEND_CLEAR;

    case GSK_PORTER_DUFF_SOURCE_OVER_DEST:
      return GSK_GPU_BLEND_OVER;

    case GSK_PORTER_DUFF_DEST:
    default:
      g_assert_not_reached ();
      return GSK_GPU_BLEND_OVER;
    }
}

static void
gsk_gpu_node_processor_add_composite_node (GskGpuRenderPass *self,
                                           GskRenderNode       *node)
{
  GskRenderNode *child;
  GskGpuImage *mask_image;
  graphene_rect_t bounds, mask_rect;
  GskPorterDuff op;
  GskGpuRenderPassBlendStorage storage;

  if (!gsk_gpu_node_processor_clip_node_bounds_and_snap_to_grid (self, node, &bounds))
    return;

  op = gsk_composite_node_get_operator (node);
  child = gsk_composite_node_get_child (node);

  /* There is a no-op operator... */
  if (op == GSK_PORTER_DUFF_DEST)
    return;
  
  gsk_gpu_render_pass_push_blend (self,
                                  gsk_gpu_blend_for_porter_duff (op),
                                  &storage);

  mask_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                         0,
                                                         &bounds,
                                                         gsk_composite_node_get_mask (node),
                                                         1,
                                                         &mask_rect);
  if (mask_image == NULL)
    return;

  if (op == GSK_PORTER_DUFF_CLEAR)
    {
      gsk_gpu_texture_op (self,
                          self->ccs,
                          &mask_rect,
                          mask_image,
                          GSK_GPU_SAMPLER_DEFAULT,
                          &mask_rect);
    }
  else
    {
      GskGpuImage *child_image;
      graphene_rect_t child_rect;

      child_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                              0,
                                                              &bounds,
                                                              child,
                                                              0,
                                                              &child_rect);
      if (child_image == NULL)
        {
          /* FIXME */
          child_image = g_object_ref (mask_image);
          /* put it far away so it won't get sampled */
          child_rect = mask_rect;
          child_rect.origin.x += 2 * mask_rect.size.width;
        }

      if (op == GSK_PORTER_DUFF_DEST_IN_SOURCE)
        {
          gsk_gpu_mask_op (self,
                           self->ccs,
                           &bounds,
                           mask_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           child_image,
                           GSK_GPU_SAMPLER_TRANSPARENT,
                           GSK_MASK_MODE_INVERTED_ALPHA,
                           &mask_rect,
                           &child_rect);
        }
      else if (!gsk_gpu_porter_duff_needs_dual_blend (op))
        {
          gsk_gpu_mask_op (self,
                           self->ccs,
                           &bounds,
                           child_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           mask_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           GSK_MASK_MODE_ALPHA,
                           &child_rect,
                           &mask_rect);
        }
      else if (gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_DUAL_BLEND))
        {
          gsk_gpu_composite_op (self,
                                self->ccs,
                                &bounds,
                                child_image,
                                GSK_GPU_SAMPLER_DEFAULT,
                                mask_image,
                                GSK_GPU_SAMPLER_DEFAULT,
                                op,
                                &bounds,
                                &child_rect,
                                &mask_rect);
        }
      else if (op == GSK_PORTER_DUFF_SOURCE)
        {
          /* SOURCE = CLEAR in mask
           *          + ADD source in mask */
          gsk_gpu_render_pass_pop_blend (self, &storage);
          gsk_gpu_render_pass_push_blend (self,
                                          GSK_GPU_BLEND_CLEAR,
                                          &storage);
          gsk_gpu_texture_op (self,
                              self->ccs,
                              &mask_rect,
                              mask_image,
                              GSK_GPU_SAMPLER_DEFAULT,
                              &mask_rect);
          gsk_gpu_render_pass_pop_blend (self, &storage);
          gsk_gpu_render_pass_push_blend (self,
                                          GSK_GPU_BLEND_ADD,
                                          &storage);
          gsk_gpu_mask_op (self,
                           self->ccs,
                           &bounds,
                           child_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           mask_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           GSK_MASK_MODE_ALPHA,
                           &child_rect,
                           &mask_rect);
        }
      else
        {
          g_warning_once ("FIXME: Implement compositing without dual blending support.");
        }

      g_object_unref (child_image);
    }

  g_object_unref (mask_image);

  gsk_gpu_render_pass_pop_blend (self, &storage);
}

static void
gsk_gpu_node_processor_add_isolation_node (GskGpuRenderPass *self,
                                           GskRenderNode       *node)
{
  GskRenderNode *child = gsk_isolation_node_get_child (node);
  GskIsolation isolations = gsk_isolation_node_get_isolations (node);

  if (isolations & GSK_ISOLATION_BACKGROUND)
    {
      if (gsk_render_node_get_copy_mode (child) != GSK_COPY_NONE ||
          gsk_render_node_clears_background (child))
        {
          gsk_gpu_frame_start_node (self->frame, child, 0);
          gsk_gpu_node_processor_add_with_offscreen (self, child);
          gsk_gpu_frame_end_node (self->frame);
          return;
        }
    }
  
  gsk_gpu_node_processor_add_node (self, child, 0);
}

static void
gsk_gpu_node_processor_add_container_node (GskGpuRenderPass *self,
                                           GskRenderNode       *node)
{
  GskRenderNode **children;
  gsize i, n_children;

  if (self->opacity < 1.0 && !gsk_container_node_is_disjoint (node))
    {
      gsk_gpu_node_processor_add_with_offscreen (self, node);
      return;
    }

  children = gsk_render_node_get_children (node, &n_children);

  if (node->fully_opaque && !gsk_container_node_is_disjoint (node) && n_children > 0)
    {
      graphene_rect_t opaque;

      /* Try to find a child that fully covers the container node */
      for (i = n_children - 1; i > 0; i--)
        {
          if (gsk_render_node_get_opaque_rect (children[i], &opaque) &&
              gsk_rect_equal (&opaque, &node->bounds))
            break;
        }
    }
  else
    i = 0;

  for (; i < n_children; i++)
    gsk_gpu_node_processor_add_node (self, children[i], i);
}

static void
gsk_gpu_node_processor_add_debug_node (GskGpuRenderPass *self,
                                       GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_node (self, gsk_debug_node_get_child (node), 0);
}

static GskGpuImage *
gsk_gpu_get_debug_node_as_image (GskGpuFrame           *frame,
                                 GskGpuAsImageFlags     flags,
                                 GdkColorState         *ccs,
                                 const graphene_rect_t *clip_bounds,
                                 const graphene_vec2_t *scale,
                                 GskRenderNode         *node,
                                 graphene_rect_t       *out_bounds)
{
  GskGpuImage *result;
  GskRenderNode *child;

  child = gsk_debug_node_get_child (node);

  gsk_gpu_frame_start_node (frame, child, 0);

  result = gsk_gpu_get_node_as_image (frame,
                                      flags,
                                      ccs,
                                      clip_bounds,
                                      scale,
                                      child,
                                      out_bounds);

  gsk_gpu_frame_end_node (frame);

  return result;
}

static const struct
{
  void                  (* process_node)                        (GskGpuRenderPass    *self,
                                                                 GskRenderNode          *node);
  GskGpuImage *         (* get_node_as_image)                   (GskGpuFrame            *self,
                                                                 GskGpuAsImageFlags      flags,
                                                                 GdkColorState          *ccs,
                                                                 const graphene_rect_t  *clip_bounds,
                                                                 const graphene_vec2_t  *scale,
                                                                 GskRenderNode          *node,
                                                                 graphene_rect_t        *out_bounds);
} nodes_vtable[] = {
  [GSK_NOT_A_RENDER_NODE] = {
    NULL,
    NULL,
  },
  [GSK_CONTAINER_NODE] = {
    gsk_gpu_node_processor_add_container_node,
    NULL,
  },
  [GSK_CAIRO_NODE] = {
    gsk_gpu_node_processor_add_cairo_node,
    gsk_gpu_get_cairo_node_as_image,
  },
  [GSK_COLOR_NODE] = {
    gsk_gpu_node_processor_add_color_node,
    NULL,
  },
  [GSK_LINEAR_GRADIENT_NODE] = {
    gsk_gpu_node_processor_add_linear_gradient_node,
    NULL,
  },
  [GSK_REPEATING_LINEAR_GRADIENT_NODE] = {
    gsk_gpu_node_processor_add_linear_gradient_node,
    NULL,
  },
  [GSK_RADIAL_GRADIENT_NODE] = {
    gsk_gpu_node_processor_add_radial_gradient_node,
    NULL,
  },
  [GSK_REPEATING_RADIAL_GRADIENT_NODE] = {
    gsk_gpu_node_processor_add_radial_gradient_node,
    NULL,
  },
  [GSK_CONIC_GRADIENT_NODE] = {
    gsk_gpu_node_processor_add_conic_gradient_node,
    NULL,
  },
  [GSK_BORDER_NODE] = {
    gsk_gpu_node_processor_add_border_node,
    NULL,
  },
  [GSK_TEXTURE_NODE] = {
    gsk_gpu_node_processor_add_texture_node,
    gsk_gpu_get_texture_node_as_image,
  },
  [GSK_INSET_SHADOW_NODE] = {
    gsk_gpu_node_processor_add_inset_shadow_node,
    NULL,
  },
  [GSK_OUTSET_SHADOW_NODE] = {
    gsk_gpu_node_processor_add_outset_shadow_node,
    NULL,
  },
  [GSK_TRANSFORM_NODE] = {
    gsk_gpu_node_processor_add_transform_node,
    NULL,
  },
  [GSK_OPACITY_NODE] = {
    gsk_gpu_node_processor_add_opacity_node,
    NULL,
  },
  [GSK_COLOR_MATRIX_NODE] = {
    gsk_gpu_node_processor_add_color_matrix_node,
    NULL,
  },
  [GSK_REPEAT_NODE] = {
    gsk_gpu_node_processor_add_repeat_node,
    NULL,
  },
  [GSK_CLIP_NODE] = {
    gsk_gpu_node_processor_add_clip_node,
    NULL,
  },
  [GSK_ROUNDED_CLIP_NODE] = {
    gsk_gpu_node_processor_add_rounded_clip_node,
    NULL,
  },
  [GSK_SHADOW_NODE] = {
    gsk_gpu_node_processor_add_shadow_node,
    NULL,
  },
  [GSK_BLEND_NODE] = {
    gsk_gpu_node_processor_add_blend_node,
    NULL,
  },
  [GSK_CROSS_FADE_NODE] = {
    gsk_gpu_node_processor_add_cross_fade_node,
    NULL,
  },
  [GSK_TEXT_NODE] = {
    gsk_gpu_node_processor_add_glyph_node,
    NULL,
  },
  [GSK_BLUR_NODE] = {
    gsk_gpu_node_processor_add_blur_node,
    NULL,
  },
  [GSK_DEBUG_NODE] = {
    gsk_gpu_node_processor_add_debug_node,
    gsk_gpu_get_debug_node_as_image,
  },
  [GSK_GL_SHADER_NODE] = {
    gsk_gpu_node_processor_add_gl_shader_node,
    NULL,
  },
  [GSK_TEXTURE_SCALE_NODE] = {
    gsk_gpu_node_processor_add_texture_scale_node,
    NULL,
  },
  [GSK_MASK_NODE] = {
    gsk_gpu_node_processor_add_mask_node,
    NULL,
  },
  [GSK_FILL_NODE] = {
    gsk_gpu_node_processor_add_fill_node,
    NULL,
  },
  [GSK_STROKE_NODE] = {
    gsk_gpu_node_processor_add_stroke_node,
    NULL,
  },
  [GSK_SUBSURFACE_NODE] = {
    gsk_gpu_node_processor_add_subsurface_node,
    gsk_gpu_get_subsurface_node_as_image,
  },
  [GSK_COMPONENT_TRANSFER_NODE] = {
    gsk_gpu_node_processor_add_component_transfer_node,
    NULL,
  },
  [GSK_COPY_NODE] = {
    gsk_gpu_node_processor_add_copy_node,
    NULL,
  },
  [GSK_PASTE_NODE] = {
    gsk_gpu_node_processor_add_paste_node,
    NULL,
  },
  [GSK_COMPOSITE_NODE] = {
    gsk_gpu_node_processor_add_composite_node,
    NULL,
  },
  [GSK_ISOLATION_NODE] = {
    gsk_gpu_node_processor_add_isolation_node,
    NULL,
  },
  [GSK_DISPLACEMENT_NODE] = {
    gsk_gpu_node_processor_add_displacement_node,
    NULL,
  },
  [GSK_ARITHMETIC_NODE] = {
    gsk_gpu_node_processor_add_arithmetic_node,
    NULL,
  },
};

static void
gsk_gpu_node_processor_add_node_untracked (GskGpuRenderPass *self,
                                           GskRenderNode       *node)
{
  GskRenderNodeType node_type;

  /* This catches the corner cases of empty nodes, so after this check
   * there's quaranteed to be at least 1 pixel that needs to be drawn
   */
  if (node->bounds.size.width == 0 || node->bounds.size.height == 0)
    return;

  if (!gsk_gpu_clip_may_intersect_rect (&self->clip, &self->offset, &node->bounds))
    return;

  node_type = gsk_render_node_get_node_type (node);
  if (node_type >= G_N_ELEMENTS (nodes_vtable))
    {
      g_critical ("unknown node type %u for %s", node_type, g_type_name_from_instance ((GTypeInstance *) node));
      return;
    }

  if (nodes_vtable[node_type].process_node)
    {
      nodes_vtable[node_type].process_node (self, node);
    }
  else
    {
      g_warning_once ("Unimplemented node '%s'",
                      g_type_name_from_instance ((GTypeInstance *) node));
      /* Maybe it's implemented in the Cairo renderer? */
      gsk_gpu_node_processor_add_cairo_node (self, node);
    }
}

/*
 * gsk_gpu_get_node_as_image:
 * @frame: frame to render in
 * @flags: flags for the image
 * @ccs: the color state to composite the image in
 * @clip_bounds: region of node that must be included in image
 * @scale: scale factor to use for the image
 * @node: the node to render
 * @pos: position in child to do tracking with or -1 for no tracking
 * @out_bounds: the actual bounds of the result
 *
 * Get the part of the node indicated by the clip bounds as an image.
 *
 * The resulting image will be in the given colorstate and premultiplied.
 *
 * It is perfectly valid for this function to return an image covering
 * a larger or smaller rectangle than the given clip bounds.
 * It can be smaller if the node is actually smaller than the clip
 * bounds and it's not necessary to create such a large offscreen, and
 * it can be larger if only part of a node is drawn but a cached image
 * for the full node (usually a texture node) already exists.
 *
 * The rectangle that is actually covered by the image is returned in
 * out_bounds.
 *
 * Returns: the image or %NULL if there was nothing to render
 **/
static GskGpuImage *
gsk_gpu_get_node_as_image (GskGpuFrame           *frame,
                           GskGpuAsImageFlags     flags,
                           GdkColorState         *ccs,
                           const graphene_rect_t *clip_bounds,
                           const graphene_vec2_t *scale,
                           GskRenderNode         *node,
                           graphene_rect_t       *out_bounds)
{
  GskRenderNodeType node_type;

  node_type = gsk_render_node_get_node_type (node);
  if (node_type >= G_N_ELEMENTS (nodes_vtable))
    {
      g_critical ("unknown node type %u for %s", node_type, g_type_name_from_instance ((GTypeInstance *) node));
      return NULL;
    }

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_TO_IMAGE) &&
      nodes_vtable[node_type].get_node_as_image)
    {
      return nodes_vtable[node_type].get_node_as_image (frame, flags, ccs, clip_bounds, scale, node, out_bounds);
    }
  else
    {
      GSK_DEBUG (FALLBACK, "Unsupported node '%s'",
                 g_type_name_from_instance ((GTypeInstance *) node));
      return gsk_gpu_get_node_as_image_via_offscreen (frame, flags, ccs, clip_bounds, scale, node, out_bounds);
    }
}

static void
gsk_gpu_node_processor_convert_to (GskGpuRenderPass   *self,
                                   GdkShaderOp            target_shader_op,
                                   GskGpuImage           *image,
                                   GdkColorState         *image_color_state,
                                   const graphene_rect_t *rect,
                                   const graphene_rect_t *tex_rect)
{
  gboolean target_premultiplied;

  switch (target_shader_op)
    {
    case GDK_SHADER_DEFAULT:
      target_premultiplied = TRUE;
      break;
    case GDK_SHADER_STRAIGHT:
      target_premultiplied = FALSE;
      break;
    case GDK_SHADER_2_PLANES:
    case GDK_SHADER_3_PLANES:
    case GDK_SHADER_3_PLANES_10BIT_LSB:
    case GDK_SHADER_3_PLANES_12BIT_LSB:
    default:
      g_return_if_reached ();
    }

  if (GDK_IS_BUILTIN_COLOR_STATE (self->ccs))
    {
      gsk_gpu_convert_builtin_op (self,
                                  image_color_state,
                                  gsk_gpu_get_acs_for_builtin (self->ccs),
                                  rect,
                                  image,
                                  GSK_GPU_SAMPLER_DEFAULT,
                                  GDK_BUILTIN_COLOR_STATE_ID (self->ccs),
                                  target_premultiplied,
                                  TRUE,
                                  tex_rect);
    }
  else if (!GDK_IS_DEFAULT_COLOR_STATE (self->ccs))
    {
      const GdkCicp *cicp = gdk_color_state_get_cicp (self->ccs);

      g_assert (cicp != NULL);

      gsk_gpu_convert_cicp_op (self,
                               image_color_state,
                               gsk_gpu_get_acs_for_cicp (self->ccs, image_color_state),
                               rect,
                               image,
                               GSK_GPU_SAMPLER_DEFAULT,
                               target_premultiplied,
                               TRUE,
                               tex_rect,
                               cicp->color_primaries,
                               cicp->transfer_function,
                               cicp->matrix_coefficients,
                               cicp->range == GDK_CICP_RANGE_NARROW ? 0 : 1);
    }
  else
    {
      gsk_gpu_convert_op (self,
                          self->ccs,
                          target_premultiplied,
                          image_color_state,
                          rect,
                          image,
                          GSK_GPU_SAMPLER_DEFAULT,
                          tex_rect);
    }
}

void
gsk_gpu_node_processor_process (GskGpuFrame           *frame,
                                GskGpuImage           *target,
                                GdkColorState         *target_color_state,
                                cairo_region_t        *clip,
                                GskRenderNode         *node,
                                const graphene_rect_t *viewport,
                                GskRenderPassType      pass_type)
{
  GskGpuRenderPass self;
  GdkColorState *ccs;
  GskGpuImage *image;
  graphene_rect_t clip_bounds, tex_rect;
  int i;

  ccs = gdk_color_state_get_rendering_color_state (target_color_state);

  if (gdk_color_state_equal (ccs, target_color_state))
    {
      gsk_gpu_occlusion_render_node (frame, target, target_color_state, pass_type, clip, viewport, node);
    }
  else
    {
      GskGpuRenderPassBlendStorage blend_storage;
      cairo_rectangle_int_t extents;

      cairo_region_get_extents (clip, &extents);

      gsk_gpu_render_pass_init (&self,
                                frame,
                                target,
                                target_color_state,
                                pass_type,
                                gdk_cairo_region_is_rectangle (clip)
                                ? GSK_GPU_LOAD_OP_DONT_CARE
                                : GSK_GPU_LOAD_OP_LOAD,
                                NULL,
                                &extents,
                                viewport);

      gsk_gpu_render_pass_push_blend (&self, GSK_GPU_BLEND_NONE, &blend_storage);

      for (i = 0; i < cairo_region_num_rectangles (clip); i++)
        {
          GskGpuRenderPassClipStorage clip_storage;
          cairo_rectangle_int_t rect;

          cairo_region_get_rectangle (clip, i, &rect);
          gsk_gpu_render_pass_push_clip_device_rect (&self, &rect, &clip_storage);

          /* Can't use gsk_gpu_node_processor_get_node_as_image () because of colorspaces */
          if (!gsk_gpu_node_processor_clip_node_bounds_and_snap_to_grid (&self, node, &clip_bounds))
            {
              gsk_gpu_render_pass_pop_clip_device_rect (&self, &clip_storage);
              continue;
            }

          image = gsk_gpu_get_node_as_image (self.frame,
                                             0,
                                             ccs,
                                             &clip_bounds,
                                             &self.scale,
                                             node,
                                             &tex_rect);
          if (image == NULL)
            {
              gsk_gpu_render_pass_pop_clip_device_rect (&self, &clip_storage);
              continue;
            }

          gsk_gpu_node_processor_convert_to (&self,
                                             gsk_gpu_image_get_shader_op (target),
                                             image,
                                             ccs,
                                             &clip_bounds,
                                             &tex_rect);

          g_object_unref (image);
          gsk_gpu_render_pass_pop_clip_device_rect (&self, &clip_storage);
        }

      gsk_gpu_render_pass_pop_blend (&self, &blend_storage);
      gsk_gpu_render_pass_finish (&self);

      cairo_region_destroy (clip);
    }
}

GskGpuImage *
gsk_gpu_node_processor_convert_image (GskGpuFrame     *frame,
                                      GdkMemoryFormat  target_format,
                                      GdkColorState   *target_color_state,
                                      GskGpuImage     *image,
                                      GdkColorState   *image_color_state)
{
  GskGpuRenderPass self;
  GskGpuImage *target, *intermediate = NULL;
  gsize width, height;
  gboolean target_shader_op, image_shader_op;
  GskGpuRenderPassBlendStorage storage;

  width = gsk_gpu_image_get_width (image);
  height = gsk_gpu_image_get_height (image);

  target = create_offscreen_image (frame,
                                   FALSE,
                                   target_format,
                                   gsk_gpu_image_get_conversion (image) == GSK_GPU_CONVERSION_SRGB,
                                   width,
                                   height);
  if (target == NULL)
    return NULL;

  target_shader_op = gsk_gpu_image_get_shader_op (target);
  image_shader_op = gsk_gpu_image_get_shader_op (image);

  /* We need to go via an intermediate colorstate */
  if (!(GDK_IS_DEFAULT_COLOR_STATE (image_color_state) && image_shader_op == GDK_SHADER_DEFAULT) &&
      !(GDK_IS_DEFAULT_COLOR_STATE (target_color_state) && target_shader_op == GDK_SHADER_DEFAULT))
    {
      GdkColorState *ccs = gdk_color_state_get_rendering_color_state (image_color_state);

      intermediate = gsk_gpu_copy_image (frame, ccs, g_object_ref (image), image_color_state, FALSE);
      if (intermediate == NULL)
        return NULL;
      image = intermediate;
      image_color_state = ccs;
      image_shader_op = GDK_SHADER_DEFAULT;
    }

  gsk_gpu_render_pass_init (&self,
                            frame,
                            target,
                            target_color_state,
                            GSK_RENDER_PASS_OFFSCREEN,
                            GSK_GPU_LOAD_OP_DONT_CARE,
                            NULL,
                            &(cairo_rectangle_int_t) { 0, 0, width, height },
                            &GRAPHENE_RECT_INIT (0, 0, width, height));

  gsk_gpu_render_pass_push_blend (&self, GSK_GPU_BLEND_NONE, &storage);

  if (GDK_IS_DEFAULT_COLOR_STATE (target_color_state) && target_shader_op == GDK_SHADER_DEFAULT)
    {
      gsk_gpu_node_processor_image_op (&self,
                                       image,
                                       image_color_state,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &GRAPHENE_RECT_INIT (0, 0, width, height),
                                       &GRAPHENE_RECT_INIT (0, 0, width, height));
    }
  else
    {
      gsk_gpu_node_processor_convert_to (&self,
                                         target_shader_op,
                                         image,
                                         image_color_state,
                                         &GRAPHENE_RECT_INIT (0, 0, width, height),
                                         &GRAPHENE_RECT_INIT (0, 0, width, height));
    }

  gsk_gpu_render_pass_pop_blend (&self, &storage);
  gsk_gpu_render_pass_finish (&self);

  g_clear_object (&intermediate);

  return target;
}

void
gsk_gpu_node_processor_add_first_node_untracked (GskGpuRenderPass *self,
                                                 GskRenderNode    *node)
{
  if (gsk_render_node_needs_blending (node))
    { 
      gsk_gpu_node_processor_add_node_untracked (self, node);
    }
  else
    {
      GskGpuRenderPassBlendStorage storage;

      gsk_gpu_render_pass_push_blend (self, GSK_GPU_BLEND_NONE, &storage);

      gsk_gpu_node_processor_add_node_untracked (self, node);

      gsk_gpu_render_pass_pop_blend (self, &storage);
    }
}

GskGpuRenderPass *
gsk_render_node_default_occlusion (GskRenderNode   *self,
                                   GskGpuOcclusion *occlusion)
{
  GskGpuRenderPass *result;

  if (gsk_render_node_needs_blending (self))
    { 
      result = gsk_gpu_occlusion_begin_rendering_transparent (occlusion);
    }
  else
    {
      /* Note that checking needs_blending alone is not enough to guarantee
       * that the background can be DONT_CARE.
       * But we also know that this rect is opaque, and that together is enough.
       */
      result = gsk_gpu_occlusion_begin_rendering_whatever (occlusion);
    }

  gsk_gpu_node_processor_add_first_node_untracked (result, self);  

  return result;
}

GskGpuRenderPass *
gsk_container_node_occlusion (GskRenderNode   *node,
                              GskGpuOcclusion *occlusion)
{
  GskGpuRenderPass *result;
  GskRenderNode **children;
  gsize n_children;
  int i;

  children = gsk_render_node_get_children (node, &n_children);

  if (n_children == 0)
    return NULL;

  for (i = n_children; i-- > 0; )
    {
      result = gsk_gpu_occlusion_try_node (occlusion, children[i], i);
      if (result)
        break;
    }

  if (i < 0)
    result = gsk_gpu_occlusion_begin_rendering_transparent (occlusion);

  for (i++; i < n_children; i++)
    gsk_gpu_node_processor_add_node (result, children[i], i);

  return result;
}


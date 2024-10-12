#include "config.h"

#include "gskgpunodeprocessorprivate.h"

#include "gskgpuborderopprivate.h"
#include "gskgpuboxshadowopprivate.h"
#include "gskgpublendmodeopprivate.h"
#include "gskgpublendopprivate.h"
#include "gskgpublitopprivate.h"
#include "gskgpubluropprivate.h"
#include "gskgpucacheprivate.h"
#include "gskgpuclearopprivate.h"
#include "gskgpuclipprivate.h"
#include "gskgpucolorizeopprivate.h"
#include "gskgpucolormatrixopprivate.h"
#include "gskgpucoloropprivate.h"
#include "gskgpuconicgradientopprivate.h"
#include "gskgpuconvertopprivate.h"
#include "gskgpuconvertcicpopprivate.h"
#include "gskgpucrossfadeopprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuglobalsopprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpulineargradientopprivate.h"
#include "gskgpumaskopprivate.h"
#include "gskgpumipmapopprivate.h"
#include "gskgpuradialgradientopprivate.h"
#include "gskgpurenderpassopprivate.h"
#include "gskgpuroundedcoloropprivate.h"
#include "gskgpuscissoropprivate.h"
#include "gskgputextureopprivate.h"
#include "gskgpuuploadopprivate.h"

#include "gskcairoblurprivate.h"
#include "gskdebugprivate.h"
#include "gskpath.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskroundedrectprivate.h"
#include "gskstrokeprivate.h"
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


typedef struct _GskGpuNodeProcessor GskGpuNodeProcessor;

typedef enum {
  /* The returned image will be sampled outside the bounds, so it is
   * important that it returns the right values.
   * In particular, opaque textures must ensure they return transparency
   * and images must not be contained in an atlas.
   */
  GSK_GPU_AS_IMAGE_SAMPLED_OUT_OF_BOUNDS = (1 << 0),
} GskGpuAsImageFlags;

typedef enum {
  GSK_GPU_GLOBAL_MATRIX  = (1 << 0),
  GSK_GPU_GLOBAL_SCALE   = (1 << 1),
  GSK_GPU_GLOBAL_CLIP    = (1 << 2),
  GSK_GPU_GLOBAL_SCISSOR = (1 << 3),
  GSK_GPU_GLOBAL_BLEND   = (1 << 4),
} GskGpuGlobals;

struct _GskGpuNodeProcessor
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

typedef struct _GskGpuFirstNodeInfo GskGpuFirstNodeInfo;

struct _GskGpuFirstNodeInfo
{
  GskGpuImage *target;
  cairo_rectangle_int_t extents;
  GskRenderPassType pass_type;
  gsize min_occlusion_pixels;
  float background_color[4];

  guint whole_area : 1;
  guint has_background : 1;
  guint has_started_rendering : 1;
};

static void             gsk_gpu_node_processor_add_node                 (GskGpuNodeProcessor            *self,
                                                                         GskRenderNode                  *node);
static gboolean         gsk_gpu_node_processor_add_first_node           (GskGpuNodeProcessor            *self,
                                                                         GskGpuFirstNodeInfo            *info,
                                                                         GskRenderNode                  *node);
static GskGpuImage *    gsk_gpu_get_node_as_image                       (GskGpuFrame                    *frame,
                                                                         GskGpuAsImageFlags              flags,
                                                                         GdkColorState                  *ccs,
                                                                         const graphene_rect_t          *clip_bounds,
                                                                         const graphene_vec2_t          *scale,
                                                                         GskRenderNode                  *node,
                                                                         graphene_rect_t                *out_bounds);

static void
gsk_gpu_node_processor_finish (GskGpuNodeProcessor *self)
{
  g_clear_pointer (&self->modelview, gsk_transform_unref);
}

static void
gsk_gpu_node_processor_init (GskGpuNodeProcessor         *self,
                             GskGpuFrame                 *frame,
                             GskGpuImage                 *target,
                             GdkColorState               *ccs,
                             const cairo_rectangle_int_t *clip,
                             const graphene_rect_t       *viewport)
{
  gsize width, height;

  width = gsk_gpu_image_get_width (target);
  height = gsk_gpu_image_get_height (target);

  self->frame = frame;
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

static void
gsk_gpu_node_processor_emit_globals_op (GskGpuNodeProcessor *self)
{
  graphene_matrix_t mvp;

  if (self->modelview)
    {
      gsk_transform_to_matrix (self->modelview, &mvp);
      graphene_matrix_multiply (&mvp, &self->projection, &mvp);
    }
  else
    graphene_matrix_init_from_matrix (&mvp, &self->projection);

  gsk_gpu_globals_op (self->frame,
                      &self->scale,
                      &mvp,
                      &self->clip.rect);

  self->pending_globals &= ~(GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP);
}

static void
gsk_gpu_node_processor_emit_scissor_op (GskGpuNodeProcessor *self)
{
  gsk_gpu_scissor_op (self->frame,
                      &self->scissor);
  self->pending_globals &= ~GSK_GPU_GLOBAL_SCISSOR;
}

static void
gsk_gpu_node_processor_emit_blend_op (GskGpuNodeProcessor *self)
{
  gsk_gpu_blend_op (self->frame, self->blend);
  self->pending_globals &= ~GSK_GPU_GLOBAL_BLEND;
}

static void
gsk_gpu_node_processor_sync_globals (GskGpuNodeProcessor *self,
                                     GskGpuGlobals        ignored)
{
  GskGpuGlobals required;

  required = self->pending_globals & ~ignored;

  if (required & (GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP))
    gsk_gpu_node_processor_emit_globals_op (self);
  if (required & GSK_GPU_GLOBAL_SCISSOR)
    gsk_gpu_node_processor_emit_scissor_op (self);
  if (required & GSK_GPU_GLOBAL_BLEND)
    gsk_gpu_node_processor_emit_blend_op (self);
}

static inline GskGpuColorStates
gsk_gpu_node_processor_color_states_explicit (GskGpuNodeProcessor *self,
                                              GdkColorState       *alt,
                                              gboolean             alt_premultiplied)
{
  return gsk_gpu_color_states_create (self->ccs,
                                      TRUE,
                                      alt,
                                      alt_premultiplied);
}

static GskGpuImage *
gsk_gpu_node_processor_init_draw (GskGpuNodeProcessor   *self,
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

  image = gsk_gpu_device_create_offscreen_image (gsk_gpu_frame_get_device (frame),
                                                 FALSE,
                                                 gdk_memory_depth_get_format (depth),
                                                 gdk_memory_depth_is_srgb (depth),
                                                 area.width, area.height);
  if (image == NULL)
    return NULL;

  gsk_gpu_node_processor_init (self,
                               frame,
                               image,
                               ccs,
                               &area,
                               viewport);

  gsk_gpu_render_pass_begin_op (frame,
                                image,
                                &area,
                                GSK_GPU_LOAD_OP_CLEAR,
                                GSK_VEC4_TRANSPARENT,
                                GSK_RENDER_PASS_OFFSCREEN);

  return image;
}

static void
gsk_gpu_node_processor_finish_draw (GskGpuNodeProcessor *self,
                                    GskGpuImage         *image)
{
  gsk_gpu_render_pass_end_op (self->frame,
                              image,
                              GSK_RENDER_PASS_OFFSCREEN);

  gsk_gpu_node_processor_finish (self);
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

static gboolean
gsk_gpu_node_processor_rect_clip_to_device (GskGpuNodeProcessor   *self,
                                            const graphene_rect_t *src,
                                            graphene_rect_t       *dest)
{
  graphene_rect_t transformed;
  float scale_x = graphene_vec2_get_x (&self->scale);
  float scale_y = graphene_vec2_get_y (&self->scale);

  switch (gsk_transform_get_fine_category (self->modelview))
    {
    case GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_FINE_TRANSFORM_CATEGORY_ANY:
    case GSK_FINE_TRANSFORM_CATEGORY_3D:
    case GSK_FINE_TRANSFORM_CATEGORY_2D:
      return FALSE;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE:
      gsk_transform_transform_bounds (self->modelview, src, &transformed);
      src = &transformed;
      break;

    case GSK_FINE_TRANSFORM_CATEGORY_IDENTITY:
    default:
      break;
    }

  dest->origin.x = src->origin.x * scale_x;
  dest->origin.y = src->origin.y * scale_y;
  dest->size.width = src->size.width * scale_x;
  dest->size.height = src->size.height * scale_y;

  return TRUE;
}

static gboolean
gsk_gpu_node_processor_rect_device_to_clip (GskGpuNodeProcessor   *self,
                                            const graphene_rect_t *src,
                                            graphene_rect_t       *dest)
{
  graphene_rect_t transformed;
  float scale_x = graphene_vec2_get_x (&self->scale);
  float scale_y = graphene_vec2_get_y (&self->scale);

  switch (gsk_transform_get_fine_category (self->modelview))
    {
    case GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_FINE_TRANSFORM_CATEGORY_ANY:
    case GSK_FINE_TRANSFORM_CATEGORY_3D:
    case GSK_FINE_TRANSFORM_CATEGORY_2D:
      return FALSE;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        GskTransform *inverse = gsk_transform_invert (gsk_transform_ref (self->modelview));
        gsk_transform_transform_bounds (inverse, src, &transformed);
        gsk_transform_unref (inverse);
        src = &transformed;
      }
      break;

    case GSK_FINE_TRANSFORM_CATEGORY_IDENTITY:
    default:
      break;
    }

  dest->origin.x = src->origin.x / scale_x;
  dest->origin.y = src->origin.y / scale_y;
  dest->size.width = src->size.width / scale_x;
  dest->size.height = src->size.height / scale_y;

  return TRUE;
}

static gboolean
gsk_gpu_node_processor_rect_to_device_shrink (GskGpuNodeProcessor   *self,
                                              const graphene_rect_t *rect,
                                              cairo_rectangle_int_t *int_rect)
{
  graphene_rect_t tmp;

  gsk_rect_init_offset (&tmp, rect, &self->offset);

  if (!gsk_gpu_node_processor_rect_clip_to_device (self, &tmp, &tmp))
    return FALSE;

  gsk_rect_to_cairo_shrink (&tmp, int_rect);

  return int_rect->width > 0 && int_rect->height > 0;
}

static gboolean
gsk_gpu_node_processor_rect_is_integer (GskGpuNodeProcessor   *self,
                                        const graphene_rect_t *rect,
                                        cairo_rectangle_int_t *int_rect)
{
  graphene_rect_t tmp;

  if (!gsk_gpu_node_processor_rect_clip_to_device (self, rect, &tmp))
    return FALSE;

  gsk_rect_to_cairo_shrink (&tmp, int_rect);

  return int_rect->x == tmp.origin.x
      && int_rect->y == tmp.origin.y
      && int_rect->width == tmp.size.width
      && int_rect->height == tmp.size.height;
}

static void
gsk_gpu_node_processor_get_clip_bounds (GskGpuNodeProcessor *self,
                                        graphene_rect_t     *out_bounds)
{
  graphene_rect_t scissor;

  if (gsk_gpu_node_processor_rect_device_to_clip (self,
                                                  &GSK_RECT_INIT_CAIRO (&self->scissor),
                                                  &scissor))
    {
      if (!gsk_rect_intersection (&scissor, &self->clip.rect.bounds, out_bounds))
        {
          g_warning ("Clipping is broken, everything is clipped, but we didn't early-exit.");
          *out_bounds = self->clip.rect.bounds;
        }
    }
  else
    {
      *out_bounds = self->clip.rect.bounds;
    }

  out_bounds->origin.x -= self->offset.x;
  out_bounds->origin.y -= self->offset.y;
}
 
static gboolean G_GNUC_WARN_UNUSED_RESULT
gsk_gpu_node_processor_clip_node_bounds (GskGpuNodeProcessor *self,
                                         GskRenderNode       *node,
                                         graphene_rect_t     *out_bounds)
{
  graphene_rect_t tmp;

  gsk_gpu_node_processor_get_clip_bounds (self, &tmp);
  
  if (!gsk_rect_intersection (&tmp, &node->bounds, out_bounds))
    return FALSE;

  return TRUE;
}

static void
gsk_gpu_node_processor_image_op (GskGpuNodeProcessor   *self,
                                 GskGpuImage           *image,
                                 GdkColorState         *image_color_state,
                                 GskGpuSampler          sampler,
                                 const graphene_rect_t *rect,
                                 const graphene_rect_t *tex_rect)
{
  gboolean straight_alpha;

  g_assert (self->pending_globals == 0);

  straight_alpha = gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_STRAIGHT_ALPHA;

  if (!GDK_IS_DEFAULT_COLOR_STATE (image_color_state))
    {
      const GdkCicp *cicp = gdk_color_state_get_cicp (image_color_state);

      g_assert (cicp != NULL);

      gsk_gpu_convert_from_cicp_op (self->frame,
                                    gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                                    cicp,
                                    gsk_gpu_color_states_create_cicp (self->ccs, TRUE, TRUE),
                                    self->opacity,
                                    straight_alpha,
                                    &self->offset,
                                    &(GskGpuShaderImage) {
                                      image,
                                      sampler,
                                      rect,
                                      tex_rect
                                    });
    }
  else if (straight_alpha ||
           self->opacity < 1.0 ||
           !gdk_color_state_equal (image_color_state, self->ccs))
    {
      gsk_gpu_convert_op (self->frame,
                          gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                          gsk_gpu_node_processor_color_states_explicit (self,
                                                                        image_color_state,
                                                                        TRUE),
                          self->opacity,
                          straight_alpha,
                          &self->offset,
                          &(GskGpuShaderImage) {
                              image,
                              sampler,
                              rect,
                              tex_rect
                          });
    }
  else
    {
      gsk_gpu_texture_op (self->frame,
                          gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                          &self->offset,
                          &(GskGpuShaderImage) {
                              image,
                              sampler,
                              rect,
                              tex_rect
                          });
    }
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

  image = gsk_gpu_device_create_offscreen_image (gsk_gpu_frame_get_device (frame),
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
                                       flags & GSK_GPU_IMAGE_SRGB);
  depth = gdk_memory_depth_merge (depth, gdk_color_state_get_depth (ccs));

  copy = gsk_gpu_device_create_offscreen_image (gsk_gpu_frame_get_device (frame),
                                                prepare_mipmap,
                                                gdk_memory_depth_get_format (depth),
                                                gdk_memory_depth_is_srgb (depth),
                                                width, height);

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_BLIT) &&
      (flags & (GSK_GPU_IMAGE_NO_BLIT | GSK_GPU_IMAGE_STRAIGHT_ALPHA | GSK_GPU_IMAGE_FILTERABLE)) == GSK_GPU_IMAGE_FILTERABLE &&
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
      GskGpuNodeProcessor other;
      graphene_rect_t rect = GRAPHENE_RECT_INIT (0, 0, width, height);

      gsk_gpu_node_processor_init (&other,
                                   frame,
                                   copy,
                                   ccs,
                                   &(cairo_rectangle_int_t) { 0, 0, width, height },
                                   &rect);

      gsk_gpu_render_pass_begin_op (other.frame,
                                    copy,
                                    &(cairo_rectangle_int_t) { 0, 0, width, height },
                                    GSK_GPU_LOAD_OP_DONT_CARE,
                                    NULL,
                                    GSK_RENDER_PASS_OFFSCREEN);

      other.blend = GSK_GPU_BLEND_NONE;
      other.pending_globals |= GSK_GPU_GLOBAL_BLEND;
      gsk_gpu_node_processor_sync_globals (&other, 0);

      gsk_gpu_node_processor_image_op (&other,
                                       image,
                                       image_cs,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &rect,
                                       &rect);

      gsk_gpu_render_pass_end_op (other.frame,
                                  copy,
                                  GSK_RENDER_PASS_OFFSCREEN);

      gsk_gpu_node_processor_finish (&other);
    }

  g_object_unref (image);

  return copy;
}

/*
 * gsk_gpu_node_processor_get_node_as_image:
 * @self: a node processor
 * @flags: flags for the image
 * @clip_bounds: (nullable): clip rectangle to use or NULL to use
 *   the current clip
 * @node: the node to turn into an image
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
gsk_gpu_node_processor_get_node_as_image (GskGpuNodeProcessor   *self,
                                          GskGpuAsImageFlags     flags,
                                          const graphene_rect_t *clip_bounds,
                                          GskRenderNode         *node,
                                          graphene_rect_t       *out_bounds)
{
  graphene_rect_t clip;

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
  gsk_rect_snap_to_grid (&clip, &self->scale, &self->offset, &clip);

  return gsk_gpu_get_node_as_image (self->frame,
                                    flags,
                                    self->ccs,
                                    &clip,
                                    &self->scale,
                                    node,
                                    out_bounds);
}

static void
gsk_gpu_node_processor_blur_op (GskGpuNodeProcessor       *self,
                                const graphene_rect_t     *rect,
                                const graphene_point_t    *shadow_offset,
                                float                      blur_radius,
                                const GdkColor            *shadow_color,
                                GskGpuImage               *source_image,
                                GdkMemoryDepth             source_depth,
                                const graphene_rect_t     *source_rect)
{
  GskGpuNodeProcessor other;
  GskGpuImage *intermediate;
  graphene_vec2_t direction;
  graphene_rect_t clip_rect, intermediate_rect;
  graphene_point_t real_offset;
  float clip_radius;

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius / 2.0);

  /* FIXME: Handle clip radius growing the clip too much */
  gsk_gpu_node_processor_get_clip_bounds (self, &clip_rect);
  clip_rect.origin.x -= shadow_offset->x;
  clip_rect.origin.y -= shadow_offset->y;
  graphene_rect_inset (&clip_rect, 0.f, -clip_radius);
  if (!gsk_rect_intersection (rect, &clip_rect, &intermediate_rect))
    return;

  gsk_rect_snap_to_grid (&intermediate_rect, &self->scale, &self->offset, &intermediate_rect);

  intermediate = gsk_gpu_node_processor_init_draw (&other,
                                                   self->frame,
                                                   self->ccs,
                                                   source_depth,
                                                   &self->scale,
                                                   &intermediate_rect);

  gsk_gpu_node_processor_sync_globals (&other, 0);

  graphene_vec2_init (&direction, blur_radius, 0.0f);
  gsk_gpu_blur_op (other.frame,
                   gsk_gpu_clip_get_shader_clip (&other.clip, &other.offset, &intermediate_rect),
                   other.ccs,
                   1,
                   &other.offset,
                   &(GskGpuShaderImage) {
                       source_image,
                       GSK_GPU_SAMPLER_TRANSPARENT,
                       &intermediate_rect,
                       source_rect
                   },
                   &direction);

  gsk_gpu_node_processor_finish_draw (&other, intermediate);

  real_offset = GRAPHENE_POINT_INIT (self->offset.x + shadow_offset->x,
                                     self->offset.y + shadow_offset->y);
  graphene_vec2_init (&direction, 0.0f, blur_radius);
  if (shadow_color)
    {
      gsk_gpu_blur_shadow_op (self->frame,
                              gsk_gpu_clip_get_shader_clip (&self->clip, &real_offset, rect),
                              self->ccs,
                              1,
                              &real_offset,
                              &(GskGpuShaderImage) {
                                  intermediate,
                                  GSK_GPU_SAMPLER_TRANSPARENT,
                                  rect,
                                  &intermediate_rect,
                              },
                              &direction,
                              shadow_color);
    }
  else
    {
      gsk_gpu_blur_op (self->frame,
                       gsk_gpu_clip_get_shader_clip (&self->clip, &real_offset, rect),
                       self->ccs,
                       1,
                       &real_offset,
                       &(GskGpuShaderImage) {
                           intermediate,
                           GSK_GPU_SAMPLER_TRANSPARENT,
                           rect,
                           &intermediate_rect,
                       },
                       &direction);
    }

  g_object_unref (intermediate);
}

static void
gsk_gpu_node_processor_add_cairo_node (GskGpuNodeProcessor *self,
                                       GskRenderNode       *node)
{
  GskGpuImage *image;
  graphene_rect_t clipped_bounds;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clipped_bounds))
    return;

  gsk_rect_snap_to_grid (&clipped_bounds, &self->scale, &self->offset, &clipped_bounds);

  gsk_gpu_node_processor_sync_globals (self, 0);

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
gsk_gpu_node_processor_add_without_opacity (GskGpuNodeProcessor *self,
                                            GskRenderNode       *node)
{
  GskGpuImage *image;
  graphene_rect_t tex_rect;

  gsk_gpu_node_processor_sync_globals (self, 0);

  image = gsk_gpu_node_processor_get_node_as_image (self,
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
gsk_gpu_node_processor_add_node_clipped (GskGpuNodeProcessor   *self,
                                         GskRenderNode         *node,
                                         const graphene_rect_t *clip_bounds)
{
  GskGpuClip old_clip;
  graphene_rect_t clip;
  cairo_rectangle_int_t scissor;

  if (gsk_rect_contains_rect (clip_bounds, &node->bounds))
    {
      gsk_gpu_node_processor_add_node (self, node);
      return;
    }

  gsk_rect_init_offset (&clip, clip_bounds, &self->offset);

  gsk_gpu_clip_init_copy (&old_clip, &self->clip);

  /* Check if we can use scissoring for the clip */
  if (gsk_gpu_node_processor_rect_is_integer (self, &clip, &scissor))
    {
      cairo_rectangle_int_t old_scissor;

      if (!gdk_rectangle_intersect (&scissor, &self->scissor, &scissor))
        return;

      old_scissor = self->scissor;

      if (gsk_gpu_clip_intersect_rect (&self->clip, &old_clip, &clip))
        {
          if (self->clip.type == GSK_GPU_CLIP_ALL_CLIPPED)
            {
              gsk_gpu_clip_init_copy (&self->clip, &old_clip);
              return;
            }
          else if ((self->clip.type == GSK_GPU_CLIP_RECT || self->clip.type == GSK_GPU_CLIP_CONTAINED) &&
                   gsk_rect_contains_rect (&self->clip.rect.bounds, &clip))
            {
              self->clip.type = GSK_GPU_CLIP_NONE;
            }

          self->scissor = scissor;
          self->pending_globals |= GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_CLIP;

          gsk_gpu_node_processor_add_node (self, node);

          gsk_gpu_clip_init_copy (&self->clip, &old_clip);
          self->scissor = old_scissor;
          self->pending_globals |= GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_CLIP;
        }
      else
        {
          self->scissor = scissor;
          self->pending_globals |= GSK_GPU_GLOBAL_SCISSOR;

          gsk_gpu_clip_init_copy (&self->clip, &old_clip);

          gsk_gpu_node_processor_add_node (self, node);

          self->scissor = old_scissor;
          self->pending_globals |= GSK_GPU_GLOBAL_SCISSOR;
        }
    }
  else
    {
      graphene_rect_t scissored_clip;

      if (gsk_gpu_node_processor_rect_device_to_clip (self,
                                                      &GSK_RECT_INIT_CAIRO (&self->scissor),
                                                      &scissored_clip))
        {
          if (!gsk_rect_intersection (&scissored_clip, &clip, &clip))
            {
              gsk_gpu_clip_init_copy (&self->clip, &old_clip);
              return;
            }
        }

      if (!gsk_gpu_clip_intersect_rect (&self->clip, &old_clip, &clip))
        {
          GskGpuImage *image;
          graphene_rect_t bounds, tex_rect;

          gsk_gpu_clip_init_copy (&self->clip, &old_clip);
          gsk_gpu_node_processor_sync_globals (self, 0);

          if (gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds) &&
              gsk_rect_intersection (&bounds, clip_bounds, &bounds))
            image = gsk_gpu_node_processor_get_node_as_image (self,
                                                              0,
                                                              &bounds,
                                                              node,
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

      if (self->clip.type == GSK_GPU_CLIP_ALL_CLIPPED)
        {
          gsk_gpu_clip_init_copy (&self->clip, &old_clip);
          return;
        }

      self->pending_globals |= GSK_GPU_GLOBAL_CLIP;

      gsk_gpu_node_processor_add_node (self, node);

      gsk_gpu_clip_init_copy (&self->clip, &old_clip);
      self->pending_globals |= GSK_GPU_GLOBAL_CLIP;
    }
}

static void
gsk_gpu_node_processor_add_clip_node (GskGpuNodeProcessor *self,
                                      GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_node_clipped (self,
                                           gsk_clip_node_get_child (node),
                                           gsk_clip_node_get_clip (node));
}

/*
 * gsk_gpu_first_node_begin_rendering:
 * @self: The node processor
 * @info: The first node info
 * @clear_color: (nullable): The color to clear to
 *
 * Begins rendering the given scissor rect and if given clears the background
 * to the given clear color.
 **/
static void
gsk_gpu_first_node_begin_rendering (GskGpuNodeProcessor *self,
                                    GskGpuFirstNodeInfo *info,
                                    float                clear_color[4])
{
  if (info->has_started_rendering)
    {
      if (clear_color &&
          (!info->has_background ||
           (info->has_background &&
            (clear_color[0] != info->background_color[0] ||
             clear_color[1] != info->background_color[1] ||
             clear_color[2] != info->background_color[2] ||
             clear_color[3] != info->background_color[3]))))
        {
          gsk_gpu_clear_op (self->frame, &self->scissor, clear_color);
        }
    }
  else
    {
      GskGpuLoadOp load_op;

      if (!info->whole_area)
        {
          info->has_background = FALSE;
          load_op = GSK_GPU_LOAD_OP_LOAD;
        }
      else if (clear_color)
        {
          info->background_color[0] = clear_color[0];
          info->background_color[1] = clear_color[1];
          info->background_color[2] = clear_color[2];
          info->background_color[3] = clear_color[3];
          info->has_background = TRUE;
          load_op = GSK_GPU_LOAD_OP_CLEAR;
        }
      else
        {
          info->has_background = FALSE;
          load_op = GSK_GPU_LOAD_OP_DONT_CARE;
        }

      info->has_started_rendering = TRUE;
      gsk_gpu_render_pass_begin_op (self->frame,
                                    info->target,
                                    &info->extents,
                                    load_op,
                                    clear_color,
                                    info->pass_type);

      if (!info->whole_area && clear_color)
        gsk_gpu_clear_op (self->frame, &self->scissor, clear_color);
    }
}

/*
 * gsk_gpu_node_processor_clip_first_node:
 * @self: the nodeprocessor
 * @info: the first pass info
 * @opaque: an opaque rectangle to clip to
 *
 * Shrinks the clip during a first node determination to only cover
 * the passed in opaque rect - or rather its intersection with the
 * previous clip.
 *
 * This can fail if the resulting scissor rect would be smaller than
 * min_occlusion_pixels and not warrant an occlusion pass.
 *
 * Adjusts scissor rect and clip, when not starting a first node,
 * you need to revert them.
 *
 * Returns: TRUE if the adjustment was successful.
 **/
static gboolean
gsk_gpu_node_processor_clip_first_node (GskGpuNodeProcessor   *self,
                                        GskGpuFirstNodeInfo   *info,
                                        const graphene_rect_t *opaque)
{
  cairo_rectangle_int_t device_clip;
  graphene_rect_t rect;

  if (!gsk_gpu_node_processor_rect_to_device_shrink (self, opaque, &device_clip) ||
      !gdk_rectangle_intersect (&device_clip, &self->scissor, &device_clip) ||
      device_clip.width * device_clip.height < info->min_occlusion_pixels)
    return FALSE;

  self->scissor = device_clip;

  gsk_gpu_node_processor_rect_device_to_clip (self,
                                              &GSK_RECT_INIT_CAIRO (&device_clip),
                                              &rect);
  gsk_gpu_clip_init_empty (&self->clip, &rect);

  return TRUE;
}

static gboolean
gsk_gpu_node_processor_add_first_node_clipped (GskGpuNodeProcessor   *self,
                                               GskGpuFirstNodeInfo   *info,
                                               const graphene_rect_t *clip,
                                               GskRenderNode         *node)
{
  GskGpuClip old_clip;
  cairo_rectangle_int_t old_scissor;

  old_scissor = self->scissor;
  gsk_gpu_clip_init_copy (&old_clip, &self->clip);

  if (!gsk_gpu_node_processor_clip_first_node (self, info, clip))
    return FALSE;

  if (gsk_gpu_node_processor_add_first_node (self,
                                             info,
                                             node))
    {
      /* don't revert clip here, the add_first_node() adjusted it to a correct value */
      return TRUE;
    }

  self->scissor = old_scissor;
  gsk_gpu_clip_init_copy (&old_clip, &self->clip);

  return FALSE;
}

static gboolean
gsk_gpu_node_processor_add_first_clip_node (GskGpuNodeProcessor *self,
                                            GskGpuFirstNodeInfo *info,
                                            GskRenderNode       *node)
{
  return gsk_gpu_node_processor_add_first_node_clipped (self,
                                                        info,
                                                        &node->bounds,
                                                        gsk_clip_node_get_child (node));
}

static void
gsk_gpu_node_processor_add_rounded_clip_node_with_mask (GskGpuNodeProcessor *self,
                                                        GskRenderNode       *node)
{
  GskGpuNodeProcessor other;
  graphene_rect_t clip_bounds, child_rect;
  GskGpuImage *child_image, *mask_image;
  GdkColor white;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip_bounds))
    return;
  gsk_rect_snap_to_grid (&clip_bounds, &self->scale, &self->offset, &clip_bounds);

  child_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                          0,
                                                          &clip_bounds,
                                                          gsk_rounded_clip_node_get_child (node),
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

  gdk_color_init (&white, self->ccs, ((float[]){ 1, 1, 1, 1 }));
  gsk_gpu_node_processor_sync_globals (&other, 0);
  gsk_gpu_rounded_color_op (other.frame,
                            gsk_gpu_clip_get_shader_clip (&other.clip, &other.offset, &node->bounds),
                            self->ccs,
                            1,
                            &other.offset,
                            gsk_rounded_clip_node_get_clip (node),
                            &white);
  gsk_gpu_node_processor_finish_draw (&other, mask_image);

  gsk_gpu_node_processor_sync_globals (self, 0);
  gsk_gpu_mask_op (self->frame,
                   gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &clip_bounds),
                   &clip_bounds,
                   &self->offset,
                   self->opacity,
                   GSK_MASK_MODE_ALPHA,
                   &(GskGpuShaderImage) {
                       child_image,
                       GSK_GPU_SAMPLER_DEFAULT,
                       NULL,
                       &child_rect,
                   },
                   &(GskGpuShaderImage) {
                       mask_image,
                       GSK_GPU_SAMPLER_DEFAULT,
                       NULL,
                       &clip_bounds,
                   });

  g_object_unref (child_image);
  g_object_unref (mask_image);
  gdk_color_finish (&white);
}

static void
gsk_gpu_node_processor_add_rounded_clip_node (GskGpuNodeProcessor *self,
                                              GskRenderNode       *node)
{
  GskGpuClip old_clip;
  GskRoundedRect clip;
  const GskRoundedRect *original_clip;
  GskRenderNode *child;
  graphene_rect_t scissor;

  child = gsk_rounded_clip_node_get_child (node);
  original_clip = gsk_rounded_clip_node_get_clip (node);

  /* Common case for entries etc: rounded solid color background.
   * And we have a shader for that */
  if (gsk_render_node_get_node_type (child) == GSK_COLOR_NODE &&
      gsk_rect_contains_rect (&child->bounds, &original_clip->bounds))
    {
      gsk_gpu_node_processor_sync_globals (self, 0);

      gsk_gpu_rounded_color_op (self->frame,
                                gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &original_clip->bounds),
                                self->ccs,
                                self->opacity,
                                &self->offset,
                                original_clip,
                                gsk_color_node_get_color2 (child));
      return;
    }

  gsk_gpu_clip_init_copy (&old_clip, &self->clip);

  clip = *original_clip;
  gsk_rounded_rect_offset (&clip, self->offset.x, self->offset.y);

  if (!gsk_gpu_clip_intersect_rounded_rect (&self->clip, &old_clip, &clip))
    {
      gsk_gpu_clip_init_copy (&self->clip, &old_clip);
      gsk_gpu_node_processor_add_rounded_clip_node_with_mask (self, node);
      return;
    }

  if (gsk_gpu_node_processor_rect_device_to_clip (self,
                                                  &GSK_RECT_INIT_CAIRO (&self->scissor),
                                                  &scissor))
    {
      GskGpuClip scissored_clip;
      if (gsk_gpu_clip_intersect_rect (&scissored_clip, &self->clip, &scissor))
        gsk_gpu_clip_init_copy (&self->clip, &scissored_clip);
    }

  if (self->clip.type == GSK_GPU_CLIP_ALL_CLIPPED)
    {
      gsk_gpu_clip_init_copy (&self->clip, &old_clip);
      return;
    }

  self->pending_globals |= GSK_GPU_GLOBAL_CLIP;

  gsk_gpu_node_processor_add_node (self, gsk_rounded_clip_node_get_child (node));

  gsk_gpu_clip_init_copy (&self->clip, &old_clip);
  self->pending_globals |= GSK_GPU_GLOBAL_CLIP;
}

static gboolean
gsk_gpu_node_processor_add_first_rounded_clip_node (GskGpuNodeProcessor *self,
                                                    GskGpuFirstNodeInfo *info,
                                                    GskRenderNode       *node)
{
  graphene_rect_t cover, clip;

  gsk_gpu_node_processor_get_clip_bounds (self, &clip);
  gsk_rounded_rect_get_largest_cover (gsk_rounded_clip_node_get_clip (node),
                                      &clip,
                                      &cover);

  return gsk_gpu_node_processor_add_first_node_clipped (self,
                                                        info,
                                                        &cover,
                                                        gsk_rounded_clip_node_get_child (node));
}

static GskTransform *
gsk_transform_dihedral (GskTransform *transform,
                        GdkDihedral   dihedral)
{
  int rotate = dihedral & 3;
  int flip = dihedral & 4;

  if (flip)
      transform = gsk_transform_scale (transform, -1.0, 1.0);

  if (rotate)
      transform = gsk_transform_rotate (transform, rotate * 90.0f);

  return transform;
}

static void
gsk_gpu_node_processor_add_transform_node (GskGpuNodeProcessor *self,
                                           GskRenderNode       *node)
{
  GskRenderNode *child;
  GskTransform *transform;
  graphene_point_t old_offset;
  graphene_vec2_t old_scale;
  GskTransform *old_modelview;
  GskGpuClip old_clip;

  child = gsk_transform_node_get_child (node);
  transform = gsk_transform_node_get_transform (node);

  switch (gsk_transform_get_fine_category (transform))
    {
    case GSK_FINE_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;
        gsk_transform_to_translate (transform, &dx, &dy);
        old_offset = self->offset;
        self->offset.x += dx;
        self->offset.y += dy;
        gsk_gpu_node_processor_add_node (self, child);
        self->offset = old_offset;
      }
      return;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float dx, dy, scale_x, scale_y;

        gsk_gpu_clip_init_copy (&old_clip, &self->clip);
        old_offset = self->offset;
        old_scale = self->scale;
        old_modelview = self->modelview;

        gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
        gsk_gpu_clip_scale (&self->clip, &old_clip, GDK_DIHEDRAL_NORMAL, scale_x, scale_y);
        self->offset.x = (self->offset.x + dx) / scale_x;
        self->offset.y = (self->offset.y + dy) / scale_y;
        graphene_vec2_init (&self->scale, scale_x, scale_y);
        graphene_vec2_multiply (&self->scale, &old_scale, &self->scale);
      }
      break;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL:
      {
        GdkDihedral dihedral, inverted;
        float xx, xy, yx, yy, dx, dy, scale_x, scale_y, old_scale_x, old_scale_y;

        gsk_gpu_clip_init_copy (&old_clip, &self->clip);
        old_offset = self->offset;
        old_scale = self->scale;
        old_modelview = gsk_transform_ref (self->modelview);

        gsk_transform_to_dihedral (transform, &dihedral, &scale_x, &scale_y, &dx, &dy);
        inverted = gdk_dihedral_invert (dihedral);
        gdk_dihedral_get_mat2 (inverted, &xx, &xy, &yx, &yy);
        gsk_gpu_clip_scale (&self->clip, &old_clip, inverted, scale_x, scale_y);
        self->offset.x = (self->offset.x + dx) / scale_x;
        self->offset.y = (self->offset.y + dy) / scale_y;
        self->offset = GRAPHENE_POINT_INIT (xx * self->offset.x + xy * self->offset.y,
                                            yx * self->offset.x + yy * self->offset.y);
        old_scale_x = graphene_vec2_get_x (&old_scale);
        old_scale_y = graphene_vec2_get_y (&old_scale);
        graphene_vec2_init (&self->scale,
                            fabs (scale_x * (old_scale_x * xx + old_scale_y * yx)),
                            fabs (scale_y * (old_scale_x * xy + old_scale_y * yy)));
        self->modelview = gsk_transform_dihedral (self->modelview, dihedral);
      }
      break;

    case GSK_FINE_TRANSFORM_CATEGORY_2D:
    case GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_FINE_TRANSFORM_CATEGORY_ANY:
    case GSK_FINE_TRANSFORM_CATEGORY_3D:
      {
        GskTransform *clip_transform;
        float scale_x, scale_y, old_pixels, new_pixels;
        graphene_rect_t scissor;

        clip_transform = gsk_transform_transform (gsk_transform_translate (NULL, &self->offset), transform);
        gsk_gpu_clip_init_copy (&old_clip, &self->clip);

        if (gsk_gpu_clip_contains_rect (&self->clip, &self->offset, &node->bounds))
          {
            gsk_gpu_clip_init_contained (&self->clip, &child->bounds);
          }
        else if (old_clip.type == GSK_GPU_CLIP_NONE)
          {
            GskTransform *inverse;
            graphene_rect_t new_bounds;
            inverse = gsk_transform_invert (gsk_transform_ref (clip_transform));
            gsk_transform_transform_bounds (inverse, &old_clip.rect.bounds, &new_bounds);
            gsk_transform_unref (inverse);
            gsk_gpu_clip_init_empty (&self->clip, &new_bounds);
          }
        else if (!gsk_gpu_clip_transform (&self->clip, &old_clip, clip_transform, &child->bounds))
          {
            GskGpuImage *image;
            graphene_rect_t tex_rect;
            gsk_transform_unref (clip_transform);
            /* This cannot loop because the next time we'll hit the branch above */
            gsk_gpu_node_processor_sync_globals (self, 0);
            image = gsk_gpu_node_processor_get_node_as_image (self,
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

        old_offset = self->offset;
        old_scale = self->scale;
        old_modelview = gsk_transform_ref (self->modelview);

        self->modelview = gsk_transform_scale (self->modelview,
                                               graphene_vec2_get_x (&self->scale),
                                               graphene_vec2_get_y (&self->scale));
        self->modelview = gsk_transform_transform (self->modelview, clip_transform);
        gsk_transform_unref (clip_transform);

        extract_scale_from_transform (self->modelview, &scale_x, &scale_y);

        old_pixels = MAX (graphene_vec2_get_x (&old_scale) * old_clip.rect.bounds.size.width,
                          graphene_vec2_get_y (&old_scale) * old_clip.rect.bounds.size.height);
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
        self->offset = *graphene_point_zero ();

        if (gsk_gpu_node_processor_rect_device_to_clip (self,
                                                        &GSK_RECT_INIT_CAIRO (&self->scissor),
                                                        &scissor))
          {
            GskGpuClip scissored_clip;
            if (gsk_gpu_clip_intersect_rect (&scissored_clip, &self->clip, &scissor))
              gsk_gpu_clip_init_copy (&self->clip, &scissored_clip);

            if (self->clip.type == GSK_GPU_CLIP_ALL_CLIPPED)
              {
                self->offset = old_offset;
                self->scale = old_scale;
                gsk_gpu_clip_init_copy (&self->clip, &old_clip);
              }
          }

      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  self->pending_globals |= GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;
  if (self->modelview != old_modelview)
    self->pending_globals |= GSK_GPU_GLOBAL_MATRIX;

  gsk_gpu_node_processor_add_node (self, child);

  self->offset = old_offset;
  self->scale = old_scale;
  gsk_gpu_clip_init_copy (&self->clip, &old_clip);
  self->pending_globals |= GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;
  if (self->modelview != old_modelview)
    {
      self->pending_globals |= GSK_GPU_GLOBAL_MATRIX;
      gsk_transform_unref (self->modelview);
      self->modelview = old_modelview;
    }
}

static gboolean
gsk_gpu_node_processor_add_first_transform_node (GskGpuNodeProcessor *self,
                                                 GskGpuFirstNodeInfo *info,
                                                 GskRenderNode       *node)
{
  GskTransform *transform;
  float dx, dy, scale_x, scale_y;
  GskGpuClip old_clip;
  graphene_point_t old_offset;
  graphene_vec2_t old_scale;
  gboolean result;

  transform = gsk_transform_node_get_transform (node);

  switch (gsk_transform_get_fine_category (transform))
    {
    case GSK_FINE_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE:
      gsk_transform_to_translate (transform, &dx, &dy);
      old_offset = self->offset;
      self->offset.x += dx;
      self->offset.y += dy;
      result = gsk_gpu_node_processor_add_first_node (self,
                                                      info,
                                                      gsk_transform_node_get_child (node));
      self->offset = old_offset;
      return result;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE:
      gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);

      gsk_gpu_clip_init_copy (&old_clip, &self->clip);
      old_offset = self->offset;
      old_scale = self->scale;

      gsk_gpu_clip_scale (&self->clip, &old_clip, GDK_DIHEDRAL_NORMAL, scale_x, scale_y);
      self->offset.x = (self->offset.x + dx) / scale_x;
      self->offset.y = (self->offset.y + dy) / scale_y;
      graphene_vec2_init (&self->scale, fabs (scale_x), fabs (scale_y));
      graphene_vec2_multiply (&self->scale, &old_scale, &self->scale);

      self->pending_globals |= GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;

      result = gsk_gpu_node_processor_add_first_node (self,
                                                      info,
                                                      gsk_transform_node_get_child (node));

      self->offset = old_offset;
      self->scale = old_scale;
      gsk_gpu_clip_init_copy (&self->clip, &old_clip);

      self->pending_globals |= GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;

      return result;

    case GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE:
    case GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL:
      {
        GdkDihedral dihedral, inverted;
        float xx, xy, yx, yy, old_scale_x, old_scale_y;
        GskTransform *old_modelview;

        gsk_gpu_clip_init_copy (&old_clip, &self->clip);
        old_offset = self->offset;
        old_scale = self->scale;
        old_modelview = gsk_transform_ref (self->modelview);

        gsk_transform_to_dihedral (transform, &dihedral, &scale_x, &scale_y, &dx, &dy);
        inverted = gdk_dihedral_invert (dihedral);
        gdk_dihedral_get_mat2 (inverted, &xx, &xy, &yx, &yy);
        gsk_gpu_clip_scale (&self->clip, &old_clip, inverted, scale_x, scale_y);
        self->offset.x = (self->offset.x + dx) / scale_x;
        self->offset.y = (self->offset.y + dy) / scale_y;
        self->offset = GRAPHENE_POINT_INIT (xx * self->offset.x + xy * self->offset.y,
                                            yx * self->offset.x + yy * self->offset.y);
        old_scale_x = graphene_vec2_get_x (&old_scale);
        old_scale_y = graphene_vec2_get_y (&old_scale);
        graphene_vec2_init (&self->scale,
                            fabs (scale_x * (old_scale_x * xx + old_scale_y * yx)),
                            fabs (scale_y * (old_scale_x * xy + old_scale_y * yy)));
        self->modelview = gsk_transform_dihedral (self->modelview, dihedral);

        self->pending_globals |= GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;

        result = gsk_gpu_node_processor_add_first_node (self,
                                                        info,
                                                        gsk_transform_node_get_child (node));

        self->offset = old_offset;
        self->scale = old_scale;
        gsk_gpu_clip_init_copy (&self->clip, &old_clip);
        gsk_transform_unref (self->modelview);
        self->modelview = old_modelview;

        self->pending_globals |= GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP;

        return result;
      }

    case GSK_FINE_TRANSFORM_CATEGORY_2D:
    case GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_FINE_TRANSFORM_CATEGORY_ANY:
    case GSK_FINE_TRANSFORM_CATEGORY_3D:
      return FALSE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static void
gsk_gpu_node_processor_add_opacity_node (GskGpuNodeProcessor *self,
                                         GskRenderNode       *node)
{
  float old_opacity = self->opacity;

  self->opacity *= gsk_opacity_node_get_opacity (node);

  gsk_gpu_node_processor_add_node (self, gsk_opacity_node_get_child (node));

  self->opacity = old_opacity;
}

static void
gsk_gpu_node_processor_add_color_node (GskGpuNodeProcessor *self,
                                       GskRenderNode       *node)
{
  cairo_rectangle_int_t int_clipped;
  graphene_rect_t rect, clipped;
  float clear_color[4];

  gsk_rect_init_offset (&rect, &node->bounds, &self->offset);
  gsk_rect_intersection (&self->clip.rect.bounds, &rect, &clipped);

  if (gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_CLEAR) &&
      gdk_color_is_opaque (gsk_color_node_get_color2 (node)) &&
      self->opacity >= 1.0 &&
      node->bounds.size.width * node->bounds.size.height > 100 * 100 && /* not worth the effort for small images */
      gsk_gpu_node_processor_rect_is_integer (self, &clipped, &int_clipped))
    {
      /* now handle all the clip */
      if (!gdk_rectangle_intersect (&int_clipped, &self->scissor, &int_clipped))
        return;

      /* we have handled the bounds, now do the corners */
      if (self->clip.type == GSK_GPU_CLIP_ROUNDED)
        {
          graphene_rect_t cover;
          GskGpuShaderClip shader_clip;
          float scale_x, scale_y;

          if (self->modelview)
            {
              /* Yuck, rounded clip and modelview. I give up. */
              gsk_gpu_color_op (self->frame,
                                gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                                self->ccs,
                                self->opacity,
                                &self->offset,
                                &node->bounds,
                                gsk_color_node_get_color2 (node));
              return;
            }

          scale_x = graphene_vec2_get_x (&self->scale);
          scale_y = graphene_vec2_get_y (&self->scale);
          clipped = GRAPHENE_RECT_INIT (int_clipped.x / scale_x, int_clipped.y / scale_y,
                                        int_clipped.width / scale_x, int_clipped.height / scale_y);
          shader_clip = gsk_gpu_clip_get_shader_clip (&self->clip, graphene_point_zero(), &clipped);
          if (shader_clip != GSK_GPU_SHADER_CLIP_NONE)
            {
              gsk_rounded_rect_get_largest_cover (&self->clip.rect, &clipped, &cover);
              int_clipped.x = ceilf (cover.origin.x * scale_x);
              int_clipped.y = ceilf (cover.origin.y * scale_y);
              int_clipped.width = floorf ((cover.origin.x + cover.size.width) * scale_x) - int_clipped.x;
              int_clipped.height = floorf ((cover.origin.y + cover.size.height) * scale_y) - int_clipped.y;
              if (int_clipped.width == 0 || int_clipped.height == 0)
                {
                  gsk_gpu_color_op (self->frame,
                                    shader_clip,
                                    self->ccs,
                                    self->opacity,
                                    graphene_point_zero (),
                                    &clipped,
                                    gsk_color_node_get_color2 (node));
                  return;
                }
              cover = GRAPHENE_RECT_INIT (int_clipped.x / scale_x, int_clipped.y / scale_y,
                                          int_clipped.width / scale_x, int_clipped.height / scale_y);
              if (clipped.origin.x != cover.origin.x)
                gsk_gpu_color_op (self->frame,
                                  shader_clip,
                                  self->ccs,
                                  self->opacity,
                                  graphene_point_zero (),
                                  &GRAPHENE_RECT_INIT (clipped.origin.x, clipped.origin.y, cover.origin.x - clipped.origin.x, clipped.size.height),
                                  gsk_color_node_get_color2 (node));
              if (clipped.origin.y != cover.origin.y)
                gsk_gpu_color_op (self->frame,
                                  shader_clip,
                                  self->ccs,
                                  self->opacity,
                                  graphene_point_zero (),
                                  &GRAPHENE_RECT_INIT (clipped.origin.x, clipped.origin.y, clipped.size.width, cover.origin.y - clipped.origin.y),
                                  gsk_color_node_get_color2 (node));
              if (clipped.origin.x + clipped.size.width != cover.origin.x + cover.size.width)
                gsk_gpu_color_op (self->frame,
                                  shader_clip,
                                  self->ccs,
                                  self->opacity,
                                  graphene_point_zero (),
                                  &GRAPHENE_RECT_INIT (cover.origin.x + cover.size.width,
                                                       clipped.origin.y,
                                                       clipped.origin.x + clipped.size.width - cover.origin.x - cover.size.width,
                                                       clipped.size.height),
                                  gsk_color_node_get_color2 (node));
              if (clipped.origin.y + clipped.size.height != cover.origin.y + cover.size.height)
                gsk_gpu_color_op (self->frame,
                                  shader_clip,
                                  self->ccs,
                                  self->opacity,
                                  graphene_point_zero (),
                                  &GRAPHENE_RECT_INIT (clipped.origin.x,
                                                       cover.origin.y + cover.size.height,
                                                       clipped.size.width,
                                                       clipped.origin.y + clipped.size.height - cover.origin.y - cover.size.height),
                                  gsk_color_node_get_color2 (node));
            }
        }

      gdk_color_to_float (gsk_color_node_get_color2 (node), self->ccs, clear_color);
      gsk_gpu_clear_op (self->frame, &int_clipped, clear_color);
      return;
    }

  gsk_gpu_color_op (self->frame,
                    gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                    self->ccs,
                    self->opacity,
                    &self->offset,
                    &node->bounds,
                    gsk_color_node_get_color2 (node));
}

static gboolean
gsk_gpu_node_processor_add_first_color_node (GskGpuNodeProcessor *self,
                                             GskGpuFirstNodeInfo *info,
                                             GskRenderNode       *node)
{
  float clear_color[4];

  if (!node->fully_opaque)
    return FALSE;

  if (!gsk_gpu_node_processor_clip_first_node (self, info, &node->bounds))
    return FALSE;

  gdk_color_to_float (gsk_color_node_get_color2 (node), self->ccs, clear_color);
  gsk_gpu_first_node_begin_rendering (self, info, clear_color);

  return TRUE;
}

static void
gsk_gpu_node_processor_add_border_node (GskGpuNodeProcessor *self,
                                        GskRenderNode       *node)
{
  gsk_gpu_border_op (self->frame,
                     gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                     self->ccs,
                     self->opacity,
                     &self->offset,
                     gsk_border_node_get_outline (node),
                     graphene_point_zero (),
                     gsk_border_node_get_widths (node),
                     gsk_border_node_get_colors2 (node));
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

  image_cs = gdk_texture_get_color_state (texture);

  if (gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_SRGB)
    {
      image_cs = gdk_color_state_get_no_srgb_tf (image_cs);
      g_assert (image_cs);
    }

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
gsk_gpu_node_processor_draw_texture_tiles (GskGpuNodeProcessor    *self,
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
  gsk_gpu_node_processor_get_clip_bounds (self, &clip_bounds);
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
              if (gsk_gpu_image_get_flags (tile) & GSK_GPU_IMAGE_SRGB)
                {
                  tile_cs = gdk_color_state_get_no_srgb_tf (tile_cs);
                  g_assert (tile_cs);
                }

              gsk_gpu_cache_cache_tile (cache, texture, lod_level, scaling_filter, y * n_width + x, tile, tile_cs);
            }

          if (need_mipmap &&
              (gsk_gpu_image_get_flags (tile) & (GSK_GPU_IMAGE_STRAIGHT_ALPHA | GSK_GPU_IMAGE_CAN_MIPMAP)) != GSK_GPU_IMAGE_CAN_MIPMAP)
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
  GskGpuNodeProcessor self;
  GskGpuImage *image;

  image = gsk_gpu_node_processor_init_draw (&self,
                                            frame,
                                            ccs,
                                            gdk_texture_get_depth (texture),
                                            scale,
                                            clip_bounds);
  if (image == NULL)
    return NULL;

  self.blend = GSK_GPU_BLEND_ADD;
  self.pending_globals |= GSK_GPU_GLOBAL_BLEND;
  gsk_gpu_node_processor_sync_globals (&self, 0);

  gsk_gpu_node_processor_draw_texture_tiles (&self,
                                             texture_bounds,
                                             texture,
                                             scaling_filter);

  gsk_gpu_node_processor_finish_draw (&self, image);

  return image;
}

static void
gsk_gpu_node_processor_add_texture_node (GskGpuNodeProcessor *self,
                                         GskRenderNode       *node)
{
  GdkColorState *image_cs;
  GskGpuImage *image;
  GdkTexture *texture;
  gboolean should_mipmap;

  texture = gsk_texture_node_get_texture (node);
  should_mipmap = texture_node_should_mipmap (node, self->frame, &self->scale);

  image = gsk_gpu_lookup_texture (self->frame, self->ccs, texture, should_mipmap, &image_cs);

  if (image == NULL)
    {
      graphene_rect_t clip, rounded_clip;

      if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip))
        return;
      gsk_rect_snap_to_grid (&clip, &self->scale, &self->offset, &rounded_clip);

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
    {
      if ((gsk_gpu_image_get_flags (image) & (GSK_GPU_IMAGE_STRAIGHT_ALPHA | GSK_GPU_IMAGE_CAN_MIPMAP)) != GSK_GPU_IMAGE_CAN_MIPMAP ||
          !gdk_color_state_equal (image_cs, self->ccs))
        {
          image = gsk_gpu_copy_image (self->frame, self->ccs, image, image_cs, TRUE);
          image_cs = self->ccs;
          gsk_gpu_cache_cache_texture_image (gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (self->frame)),
                                             texture,
                                             image,
                                             image_cs);
        }

      if (!(gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_MIPMAP))
        gsk_gpu_mipmap_op (self->frame, image);

      gsk_gpu_node_processor_image_op (self,
                                       image,
                                       image_cs,
                                       GSK_GPU_SAMPLER_MIPMAP_DEFAULT,
                                       &node->bounds,
                                       &node->bounds);
    }
  else
    {
      gsk_gpu_node_processor_image_op (self,
                                       image,
                                       image_cs,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &node->bounds,
                                       &node->bounds);
    }

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
    return gsk_gpu_get_node_as_image_via_offscreen (frame, flags, ccs, clip_bounds, scale, node, out_bounds);

  if (!gdk_color_state_equal (ccs, image_cs) ||
      gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_STRAIGHT_ALPHA ||
      ((flags & GSK_GPU_AS_IMAGE_SAMPLED_OUT_OF_BOUNDS) &&
       gdk_memory_format_alpha (gsk_gpu_image_get_format (image)) == GDK_MEMORY_ALPHA_OPAQUE))
    {
      image = gsk_gpu_copy_image (frame, ccs, image, image_cs, FALSE);
      gsk_gpu_cache_cache_texture_image (gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (frame)),
                                         texture,
                                         image,
                                         ccs);
    }

  *out_bounds = node->bounds;
  return image;
}

static void
gsk_gpu_node_processor_add_texture_scale_node (GskGpuNodeProcessor *self,
                                               GskRenderNode       *node)
{
  GskGpuImage *image;
  GdkTexture *texture;
  GdkColorState *image_cs;
  GskScalingFilter scaling_filter;
  gboolean need_mipmap, need_offscreen;

  texture = gsk_texture_scale_node_get_texture (node);
  scaling_filter = gsk_texture_scale_node_get_filter (node);
  need_mipmap = scaling_filter == GSK_SCALING_FILTER_TRILINEAR;
  image = gsk_gpu_lookup_texture (self->frame, self->ccs, texture, need_mipmap, &image_cs);

  need_offscreen = image == NULL ||
                   self->modelview != NULL ||
                   !graphene_vec2_equal (&self->scale, graphene_vec2_one ());

  if (need_offscreen)
    {
      GskGpuImage *offscreen;
      graphene_rect_t clip_bounds;

      gsk_gpu_node_processor_get_clip_bounds (self, &clip_bounds);
      /* first round to pixel boundaries, so we make sure the full pixels are covered */
      gsk_rect_snap_to_grid (&clip_bounds, &self->scale, &self->offset, &clip_bounds);
      /* then expand by half a pixel so that pixels needed for eventual linear
       * filtering are available */
      graphene_rect_inset (&clip_bounds, -0.5, -0.5);
      /* finally, round to full pixels */
      gsk_rect_round_larger (&clip_bounds);
      /* now intersect with actual node bounds */
      if (!gsk_rect_intersection (&clip_bounds, &node->bounds, &clip_bounds))
        {
          g_object_unref (image);
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
          g_object_unref (image);
        }
      gsk_gpu_texture_op (self->frame,
                          gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                          &self->offset,
                          &(GskGpuShaderImage) {
                              offscreen,
                              GSK_GPU_SAMPLER_DEFAULT,
                              &node->bounds,
                              &clip_bounds
                          });
      g_object_unref (offscreen);
      return;
    }

  if ((gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_STRAIGHT_ALPHA) ||
      (need_mipmap && !(gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_CAN_MIPMAP)) ||
      !gdk_color_state_equal (image_cs, self->ccs))
    {
      image = gsk_gpu_copy_image (self->frame, self->ccs, image, image_cs, need_mipmap);
      image_cs = self->ccs;
      gsk_gpu_cache_cache_texture_image (gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (self->frame)),
                                         texture,
                                         image,
                                         image_cs);
    }

  if (need_mipmap && !(gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_MIPMAP))
    gsk_gpu_mipmap_op (self->frame, image);

  gsk_gpu_texture_op (self->frame,
                      gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                      &self->offset,
                      &(GskGpuShaderImage) {
                          image,
                          gsk_gpu_sampler_for_scaling_filter (scaling_filter),
                          &node->bounds,
                          &node->bounds,
                      });

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
gsk_gpu_node_processor_add_inset_shadow_node (GskGpuNodeProcessor *self,
                                              GskRenderNode       *node)
{
  float spread, blur_radius;
  const GdkColor *color;

  color = gsk_inset_shadow_node_get_color2 (node);
  spread = gsk_inset_shadow_node_get_spread (node);
  blur_radius = gsk_inset_shadow_node_get_blur_radius (node);

  if (blur_radius < 0.01)
    {
      GdkColor colors[4];

      for (int i = 0; i < 4; i++)
        gdk_color_init_copy (&colors[i], color);

      gsk_gpu_border_op (self->frame,
                         gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                         self->ccs,
                         self->opacity,
                         &self->offset,
                         gsk_inset_shadow_node_get_outline (node),
                         gsk_inset_shadow_node_get_offset (node),
                         (float[4]) { spread, spread, spread, spread },
                         colors);

      for (int i = 0; i < 4; i++)
        gdk_color_finish (&colors[i]);
    }
  else
    {
      gsk_gpu_box_shadow_op (self->frame,
                             gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                             self->ccs,
                             self->opacity,
                             &self->offset,
                             TRUE,
                             &node->bounds,
                             gsk_inset_shadow_node_get_outline (node),
                             gsk_inset_shadow_node_get_offset (node),
                             spread,
                             blur_radius,
                             color);
    }
}

static void
gsk_gpu_node_processor_add_outset_shadow_node (GskGpuNodeProcessor *self,
                                               GskRenderNode       *node)
{
  float spread, blur_radius;
  const GdkColor *color;
  const graphene_point_t *offset;

  color = gsk_outset_shadow_node_get_color2 (node);
  spread = gsk_outset_shadow_node_get_spread (node);
  blur_radius = gsk_outset_shadow_node_get_blur_radius (node);
  offset = gsk_outset_shadow_node_get_offset (node);

  if (blur_radius < 0.01)
    {
      GskRoundedRect outline;
      GdkColor colors[4];

      gsk_rounded_rect_init_copy (&outline, gsk_outset_shadow_node_get_outline (node));
      gsk_rounded_rect_shrink (&outline, -spread, -spread, -spread, -spread);
      gsk_rect_init_offset (&outline.bounds, &outline.bounds, offset);

      for (int i = 0; i < 4; i++)
        gdk_color_init_copy (&colors[i], color);

      gsk_gpu_border_op (self->frame,
                         gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                         self->ccs,
                         self->opacity,
                         &self->offset,
                         &outline,
                         &GRAPHENE_POINT_INIT (- offset->x, - offset->y),
                         (float[4]) { spread, spread, spread, spread },
                         colors);

      for (int i = 0; i < 4; i++)
        gdk_color_finish (&colors[i]);
    }
  else
    {
      gsk_gpu_box_shadow_op (self->frame,
                             gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                             self->ccs,
                             self->opacity,
                             &self->offset,
                             FALSE,
                             &node->bounds,
                             gsk_outset_shadow_node_get_outline (node),
                             offset,
                             spread,
                             blur_radius,
                             color);
    }
}

typedef void (* GradientOpFunc) (GskGpuNodeProcessor  *self,
                                 GdkColorState        *target,
                                 GskRenderNode        *node,
                                 const GskColorStop2  *stops,
                                 gsize                 n_stops);

static void
gsk_gpu_node_processor_add_gradient_node (GskGpuNodeProcessor *self,
                                          GskRenderNode       *node,
                                          GdkColorState       *ics,
                                          const GskColorStop2 *stops,
                                          gsize                n_stops,
                                          GradientOpFunc       func)
{
  GskColorStop2 real_stops[7];
  GskGpuNodeProcessor other;
  graphene_rect_t bounds;
  gsize i, j;
  GskGpuImage *image;
  GdkColorState *target;

  if (GDK_IS_DEFAULT_COLOR_STATE (ics))
    target = self->ccs;
  else
    target = ics;

  if (n_stops < 8 && GDK_IS_DEFAULT_COLOR_STATE (ics))
    {
      func (self, target, node, stops, n_stops);
      return;
    }

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds))
    return;
  gsk_rect_snap_to_grid (&bounds, &self->scale, &self->offset, &bounds);

  image = gsk_gpu_node_processor_init_draw (&other,
                                            self->frame,
                                            self->ccs,
                                            gdk_memory_depth_merge (gdk_color_state_get_depth (self->ccs),
                                                                    gsk_render_node_get_preferred_depth (node)),
                                            &self->scale,
                                            &bounds);

  other.blend = GSK_GPU_BLEND_ADD;
  other.pending_globals |= GSK_GPU_GLOBAL_BLEND;
  gsk_gpu_node_processor_sync_globals (&other, 0);

  for (i = 0; i < n_stops; /* happens inside the loop */)
    {
      if (i == 0)
        {
          real_stops[0].offset = stops[i].offset;
          gdk_color_init_copy (&real_stops[i].color, &stops[i].color);
          i++;
        }
      else
        {
          real_stops[0].offset = stops[i-1].offset;
          gdk_color_init_copy (&real_stops[0].color, &stops[i - 1].color);
          real_stops[0].color.alpha *= 0;
        }
      for (j = 1; j < 6 && i < n_stops; j++)
        {
          real_stops[j].offset = stops[i].offset;
          gdk_color_init_copy (&real_stops[j].color, &stops[i].color);
          i++;
        }
      if (i == n_stops - 1)
        {
          g_assert (j == 6);
          real_stops[j].offset = stops[i].offset;
          gdk_color_init_copy (&real_stops[j].color, &stops[i].color);
          j++;
          i++;
        }
      else if (i < n_stops)
        {
          real_stops[j].offset = stops[i].offset;
          gdk_color_init_copy (&real_stops[j].color, &stops[i].color);
          real_stops[j].color.alpha *= 0;
          j++;
        }

      func (&other, target, node, real_stops, j);
    }

  gsk_gpu_node_processor_finish_draw (&other, image);

  if (GDK_IS_DEFAULT_COLOR_STATE (ics))
    {
      gsk_gpu_texture_op (self->frame,
                          gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &bounds),
                          &self->offset,
                          &(GskGpuShaderImage) {
                            image,
                            GSK_GPU_SAMPLER_DEFAULT,
                            &node->bounds,
                            &bounds
                          });
    }
  else
    {
      const GdkCicp *cicp = gdk_color_state_get_cicp (ics);

      g_assert (cicp != NULL);

      gsk_gpu_convert_from_cicp_op (self->frame,
                                    gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &bounds),
                                    cicp,
                                    gsk_gpu_color_states_create_cicp (self->ccs, TRUE, TRUE),
                                    1,
                                    TRUE,
                                    &self->offset,
                                    &(GskGpuShaderImage) {
                                      image,
                                      GSK_GPU_SAMPLER_DEFAULT,
                                      &node->bounds,
                                      &bounds
                                    });
    }

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_linear_gradient_op (GskGpuNodeProcessor  *self,
                                           GdkColorState        *target,
                                           GskRenderNode        *node,
                                           const GskColorStop2  *stops,
                                           gsize                 n_stops)
{
  gsk_gpu_linear_gradient_op (self->frame,
                              gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                              target,
                              self->opacity,
                              &self->offset,
                              gsk_linear_gradient_node_get_interpolation_color_state (node),
                              gsk_linear_gradient_node_get_hue_interpolation (node),
                              GSK_RENDER_NODE_TYPE (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE,
                              &node->bounds,
                              gsk_linear_gradient_node_get_start (node),
                              gsk_linear_gradient_node_get_end (node),
                              stops,
                              n_stops);
}

static void
gsk_gpu_node_processor_add_linear_gradient_node (GskGpuNodeProcessor *self,
                                                 GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_gradient_node (self,
                                            node,
                                            gsk_linear_gradient_node_get_interpolation_color_state (node),
                                            gsk_linear_gradient_node_get_color_stops2 (node),
                                            gsk_linear_gradient_node_get_n_color_stops (node),
                                            gsk_gpu_node_processor_linear_gradient_op);
}

static void
gsk_gpu_node_processor_radial_gradient_op (GskGpuNodeProcessor  *self,
                                           GdkColorState        *target,
                                           GskRenderNode        *node,
                                           const GskColorStop2  *stops,
                                           gsize                 n_stops)
{
  gsk_gpu_radial_gradient_op (self->frame,
                              gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                              target,
                              self->opacity,
                              &self->offset,
                              gsk_radial_gradient_node_get_interpolation_color_state (node),
                              gsk_radial_gradient_node_get_hue_interpolation (node),
                              GSK_RENDER_NODE_TYPE (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE,
                              &node->bounds,
                              gsk_radial_gradient_node_get_center (node),
                              &GRAPHENE_POINT_INIT (
                                  gsk_radial_gradient_node_get_hradius (node),
                                  gsk_radial_gradient_node_get_vradius (node)
                              ),
                              gsk_radial_gradient_node_get_start (node),
                              gsk_radial_gradient_node_get_end (node),
                              stops,
                              n_stops);
}

static void
gsk_gpu_node_processor_add_radial_gradient_node (GskGpuNodeProcessor *self,
                                                 GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_gradient_node (self,
                                            node,
                                            gsk_radial_gradient_node_get_interpolation_color_state (node),
                                            gsk_radial_gradient_node_get_color_stops2 (node),
                                            gsk_radial_gradient_node_get_n_color_stops (node),
                                            gsk_gpu_node_processor_radial_gradient_op);
}

static void
gsk_gpu_node_processor_conic_gradient_op (GskGpuNodeProcessor  *self,
                                          GdkColorState        *target,
                                          GskRenderNode        *node,
                                          const GskColorStop2  *stops,
                                          gsize                 n_stops)
{
  gsk_gpu_conic_gradient_op (self->frame,
                             gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                             target,
                             self->opacity,
                             &self->offset,
                             gsk_conic_gradient_node_get_interpolation_color_state (node),
                             gsk_conic_gradient_node_get_hue_interpolation (node),
                             &node->bounds,
                             gsk_conic_gradient_node_get_center (node),
                             gsk_conic_gradient_node_get_angle (node),
                             stops,
                             n_stops);
}

static void
gsk_gpu_node_processor_add_conic_gradient_node (GskGpuNodeProcessor *self,
                                                GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_gradient_node (self,
                                            node,
                                            gsk_conic_gradient_node_get_interpolation_color_state (node),
                                            gsk_conic_gradient_node_get_color_stops2 (node),
                                            gsk_conic_gradient_node_get_n_color_stops (node),
                                            gsk_gpu_node_processor_conic_gradient_op);
}

static void
gsk_gpu_node_processor_add_blur_node (GskGpuNodeProcessor *self,
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
      gsk_gpu_node_processor_add_node (self, child);
      return;
    }

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius / 2.0);
  gsk_gpu_node_processor_get_clip_bounds (self, &clip_rect);
  graphene_rect_inset (&clip_rect, -clip_radius, -clip_radius);
  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    GSK_GPU_AS_IMAGE_SAMPLED_OUT_OF_BOUNDS,
                                                    &clip_rect,
                                                    child,
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
                                                               gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_SRGB),
                                  &tex_rect);

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_shadow_node (GskGpuNodeProcessor *self,
                                        GskRenderNode       *node)
{
  GskGpuImage *image;
  graphene_rect_t clip_bounds, tex_rect;
  GskRenderNode *child;
  gsize i, n_shadows;

  n_shadows = gsk_shadow_node_get_n_shadows (node);
  child = gsk_shadow_node_get_child (node);
  /* enlarge clip for shadow offsets */
  gsk_gpu_node_processor_get_clip_bounds (self, &clip_bounds);
  clip_bounds = GRAPHENE_RECT_INIT (clip_bounds.origin.x - node->bounds.size.width + child->bounds.size.width - node->bounds.origin.x + child->bounds.origin.x,
                                    clip_bounds.origin.y - node->bounds.size.height + child->bounds.size.height - node->bounds.origin.y + child->bounds.origin.y,
                                    clip_bounds.size.width + node->bounds.size.width - child->bounds.size.width,
                                    clip_bounds.size.height + node->bounds.size.height - child->bounds.size.height);

  image = gsk_gpu_node_processor_get_node_as_image (self,
                                                    GSK_GPU_AS_IMAGE_SAMPLED_OUT_OF_BOUNDS,
                                                    &clip_bounds, 
                                                    child,
                                                    &tex_rect);
  if (image == NULL)
    return;

  for (i = 0; i < n_shadows; i++)
    {
      const GskShadow2 *shadow = gsk_shadow_node_get_shadow2 (node, i);

      if (shadow->radius == 0)
        {
          graphene_point_t shadow_offset = GRAPHENE_POINT_INIT (self->offset.x + shadow->offset.x,
                                                                self->offset.y + shadow->offset.y);

          gsk_gpu_colorize_op (self->frame,
                               gsk_gpu_clip_get_shader_clip (&self->clip, &shadow_offset, &child->bounds),
                               self->ccs,
                               1,
                               &shadow_offset,
                               &(GskGpuShaderImage) {
                                   image,
                                   GSK_GPU_SAMPLER_TRANSPARENT,
                                   &child->bounds,
                                   &tex_rect,
                               },
                               &shadow->color);
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
                                                                       gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_SRGB),
                                          &tex_rect);
        }
    }

  gsk_gpu_texture_op (self->frame,
                      gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &child->bounds),
                      &self->offset,
                      &(GskGpuShaderImage) {
                          image,
                          GSK_GPU_SAMPLER_DEFAULT,
                          &child->bounds,
                          &tex_rect,
                      });

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_gl_shader_node (GskGpuNodeProcessor *self,
                                           GskRenderNode       *node)
{
  gsk_gpu_color_op (self->frame,
                    gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                    self->ccs,
                    self->opacity,
                    &self->offset,
                    &node->bounds,
                    &GDK_COLOR_SRGB (1, 105/255., 180/255., 1));
}

static void
gsk_gpu_node_processor_add_blend_node (GskGpuNodeProcessor *self,
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
                                                           &bottom_rect);
  top_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                        0,
                                                        NULL,
                                                        top_child,
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

  gsk_gpu_blend_mode_op (self->frame,
                         gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                         &node->bounds,
                         &self->offset,
                         self->opacity,
                         gsk_blend_node_get_blend_mode (node),
                         &(GskGpuShaderImage) {
                             bottom_image,
                             GSK_GPU_SAMPLER_DEFAULT,
                             NULL,
                             &bottom_rect
                         },
                         &(GskGpuShaderImage) {
                             top_image,
                             GSK_GPU_SAMPLER_DEFAULT,
                             NULL,
                             &top_rect
                         });

  g_object_unref (top_image);
  g_object_unref (bottom_image);
}

static void
gsk_gpu_node_processor_add_cross_fade_node (GskGpuNodeProcessor *self,
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
      gsk_gpu_node_processor_add_node (self, start_child);
      return;
    }
  if (progress >= 1.0)
    {
      gsk_gpu_node_processor_add_node (self, end_child);
      return;
    }

  start_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                          0,
                                                          NULL,
                                                          start_child,
                                                          &start_rect);
  end_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                        0,
                                                        NULL,
                                                        end_child,
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

  gsk_gpu_cross_fade_op (self->frame,
                         gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                         &node->bounds,
                         &self->offset,
                         self->opacity,
                         progress,
                         &(GskGpuShaderImage) {
                             start_image,
                             GSK_GPU_SAMPLER_DEFAULT,
                             NULL,
                             &start_rect
                         },
                         &(GskGpuShaderImage) {
                             end_image,
                             GSK_GPU_SAMPLER_DEFAULT,
                             NULL,
                             &end_rect
                         });

  g_object_unref (end_image);
  g_object_unref (start_image);
}

static void
gsk_gpu_node_processor_add_mask_node (GskGpuNodeProcessor *self,
                                      GskRenderNode       *node)
{
  GskRenderNode *source_child, *mask_child;
  GskGpuImage *mask_image;
  graphene_rect_t bounds, mask_rect;
  GskMaskMode mask_mode;

  source_child = gsk_mask_node_get_source (node);
  mask_child = gsk_mask_node_get_mask (node);
  mask_mode = gsk_mask_node_get_mask_mode (node);

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &bounds))
    return;

  mask_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                         0,
                                                         &bounds,
                                                         mask_child,
                                                         &mask_rect);
  if (mask_image == NULL)
    {
      if (mask_mode == GSK_MASK_MODE_INVERTED_ALPHA)
        gsk_gpu_node_processor_add_node (self, source_child);
      return;
    }

  if (gsk_render_node_get_node_type (source_child) == GSK_COLOR_NODE &&
      mask_mode == GSK_MASK_MODE_ALPHA)
    {
      gsk_gpu_colorize_op (self->frame,
                           gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                           self->ccs,
                           self->opacity,
                           &self->offset,
                           &(GskGpuShaderImage) {
                               mask_image,
                               GSK_GPU_SAMPLER_DEFAULT,
                               &node->bounds,
                               &mask_rect,
                           },
                           gsk_color_node_get_color2 (source_child));
    }
  else
    {
      GskGpuImage *source_image;
      graphene_rect_t source_rect;

      source_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                               0,
                                                               &bounds,
                                                               source_child,
                                                               &source_rect);
      if (source_image == NULL)
        {
          g_object_unref (mask_image);
          return;
        }

      gsk_gpu_mask_op (self->frame,
                       gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                       &node->bounds,
                       &self->offset,
                       self->opacity,
                       mask_mode,
                       &(GskGpuShaderImage) {
                           source_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           NULL,
                           &source_rect,
                       },
                       &(GskGpuShaderImage) {
                           mask_image,
                           GSK_GPU_SAMPLER_DEFAULT,
                           NULL,
                           &mask_rect,
                       });

      g_object_unref (source_image);
    }

  g_object_unref (mask_image);
}

static void
gsk_gpu_node_processor_add_glyph_node (GskGpuNodeProcessor *self,
                                       GskRenderNode       *node)
{
  GskGpuCache *cache;
  const PangoGlyphInfo *glyphs;
  PangoFont *font;
  graphene_point_t offset;
  guint i, num_glyphs;
  float scale;
  float align_scale_x, align_scale_y;
  float inv_align_scale_x, inv_align_scale_y;
  unsigned int flags_mask;
  const float inv_pango_scale = 1.f / PANGO_SCALE;
  cairo_hint_style_t hint_style;
  const GdkColor *color;
  GdkColorState *alt;
  GskGpuColorStates color_states;
  GdkColor color2;
  GskGpuShaderClip node_clip;

  if (self->opacity < 1.0 &&
      gsk_text_node_has_color_glyphs (node))
    {
      gsk_gpu_node_processor_add_without_opacity (self, node);
      return;
    }

  cache = gsk_gpu_device_get_cache (gsk_gpu_frame_get_device (self->frame));

  num_glyphs = gsk_text_node_get_num_glyphs (node);
  glyphs = gsk_text_node_get_glyphs (node, NULL);
  font = gsk_text_node_get_font (node);
  offset = *gsk_text_node_get_offset (node);
  hint_style = gsk_text_node_get_font_hint_style (node);
  color = gsk_text_node_get_color2 (node);

  alt = gsk_gpu_color_states_find (self->ccs, color);
  color_states = gsk_gpu_color_states_create (self->ccs, TRUE, alt, FALSE);
  gdk_color_convert (&color2, alt, color);

  node_clip = gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),

  offset.x += self->offset.x;
  offset.y += self->offset.y;

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

  inv_align_scale_x = 1 / align_scale_x;
  inv_align_scale_y = 1 / align_scale_y;

  for (i = 0; i < num_glyphs; i++)
    {
      GskGpuImage *image;
      graphene_rect_t glyph_bounds, glyph_tex_rect;
      graphene_point_t glyph_offset, glyph_origin;
      GskGpuGlyphLookupFlags flags;
      GskGpuShaderClip glyph_clip;

      glyph_origin = GRAPHENE_POINT_INIT (offset.x + glyphs[i].geometry.x_offset * inv_pango_scale,
                                          offset.y + glyphs[i].geometry.y_offset * inv_pango_scale);

      glyph_origin.x = floorf (glyph_origin.x * align_scale_x + 0.5f);
      glyph_origin.y = floorf (glyph_origin.y * align_scale_y + 0.5f);
      flags = (((int) glyph_origin.x & 3) | (((int) glyph_origin.y & 3) << 2)) & flags_mask;
      glyph_origin.x *= inv_align_scale_x;
      glyph_origin.y *= inv_align_scale_y;

      image = gsk_gpu_cache_lookup_glyph_image (cache,
                                                 self->frame,
                                                 font,
                                                 glyphs[i].glyph,
                                                 flags,
                                                 scale,
                                                 &glyph_bounds,
                                                 &glyph_offset);

      glyph_tex_rect = GRAPHENE_RECT_INIT (-glyph_bounds.origin.x / scale,
                                           -glyph_bounds.origin.y / scale,
                                           gsk_gpu_image_get_width (image) / scale,
                                           gsk_gpu_image_get_height (image) / scale);
      glyph_bounds = GRAPHENE_RECT_INIT (0,
                                         0,
                                         glyph_bounds.size.width / scale,
                                         glyph_bounds.size.height / scale);
      glyph_origin = GRAPHENE_POINT_INIT (glyph_origin.x - glyph_offset.x / scale,
                                          glyph_origin.y - glyph_offset.y / scale);

      if (node_clip == GSK_GPU_SHADER_CLIP_NONE)
        glyph_clip = GSK_GPU_SHADER_CLIP_NONE;
      else
        glyph_clip = gsk_gpu_clip_get_shader_clip (&self->clip, &glyph_origin, &glyph_bounds);

      if (glyphs[i].attr.is_color)
        gsk_gpu_texture_op (self->frame,
                            glyph_clip,
                            &glyph_origin,
                            &(GskGpuShaderImage) {
                                image,
                                GSK_GPU_SAMPLER_DEFAULT,
                                &glyph_bounds,
                                &glyph_tex_rect
                            });
      else
        gsk_gpu_colorize_op2 (self->frame,
                              glyph_clip,
                              color_states,
                              self->opacity,
                              &glyph_origin,
                              &(GskGpuShaderImage) {
                                  image,
                                  GSK_GPU_SAMPLER_DEFAULT,
                                  &glyph_bounds,
                                  &glyph_tex_rect
                              },
                              &color2);

      offset.x += glyphs[i].geometry.width * inv_pango_scale;
    }

  gdk_color_finish (&color2);
}

static void
gsk_gpu_node_processor_add_color_matrix_node (GskGpuNodeProcessor *self,
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
                                                    &tex_rect);
  if (image == NULL)
    return;

  gsk_gpu_color_matrix_op (self->frame,
                           gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                           gsk_gpu_node_processor_color_states_explicit (self, self->ccs, FALSE),
                           &self->offset,
                           &(GskGpuShaderImage) {
                               image,
                               GSK_GPU_SAMPLER_DEFAULT,
                               &node->bounds,
                               &tex_rect,
                           },
                           color_matrix,
                           gsk_color_matrix_node_get_color_offset (node));

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_repeat_tile (GskGpuNodeProcessor    *self,
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

  GSK_DEBUG (FALLBACK, "Offscreening node '%s' for tiling", g_type_name_from_instance ((GTypeInstance *) child));
  image = gsk_gpu_node_processor_create_offscreen (self->frame,
                                                   self->ccs,
                                                   &self->scale,
                                                   &clipped_child_bounds,
                                                   child);

  g_return_if_fail (image);

  gsk_gpu_texture_op (self->frame,
                      gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                      &self->offset,
                      &(GskGpuShaderImage) {
                          image,
                          GSK_GPU_SAMPLER_REPEAT,
                          rect,
                          &GRAPHENE_RECT_INIT (
                              clipped_child_bounds.origin.x + x * child_bounds->size.width,
                              clipped_child_bounds.origin.y + y * child_bounds->size.height,
                              clipped_child_bounds.size.width,
                              clipped_child_bounds.size.height
                          )
                      });

  g_object_unref (image);
}

static void
gsk_gpu_node_processor_add_repeat_node (GskGpuNodeProcessor *self,
                                        GskRenderNode       *node)
{
  GskRenderNode *child;
  const graphene_rect_t *child_bounds;
  graphene_rect_t bounds;
  float tile_left, tile_right, tile_top, tile_bottom;
  gboolean avoid_offscreen;

  child = gsk_repeat_node_get_child (node);
  child_bounds = gsk_repeat_node_get_child_bounds (node);
  if (gsk_rect_is_empty (child_bounds))
    return;

  gsk_gpu_node_processor_get_clip_bounds (self, &bounds);
  if (!gsk_rect_intersection (&bounds, &node->bounds, &bounds))
    return;

  tile_left = (bounds.origin.x - child_bounds->origin.x) / child_bounds->size.width;
  tile_right = (bounds.origin.x + bounds.size.width - child_bounds->origin.x) / child_bounds->size.width;
  tile_top = (bounds.origin.y - child_bounds->origin.y) / child_bounds->size.height;
  tile_bottom = (bounds.origin.y + bounds.size.height - child_bounds->origin.y) / child_bounds->size.height;
  avoid_offscreen = !gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_REPEAT);

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
                                                       &clip_bounds);
            }
        }

      self->offset = old_offset;
    }
}

typedef struct _FillData FillData;
struct _FillData
{
  GskPath *path;
  GdkColor color;
  GskFillRule fill_rule;
};

static void
gsk_fill_data_free (gpointer data)
{
  FillData *fill = data;

  gdk_color_finish (&fill->color);
  gsk_path_unref (fill->path);
  g_free (fill);
}

static void
gsk_gpu_node_processor_fill_path (gpointer  data,
                                  cairo_t  *cr)
{
  FillData *fill = data;

  switch (fill->fill_rule)
  {
    case GSK_FILL_RULE_WINDING:
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING);
      break;
    case GSK_FILL_RULE_EVEN_ODD:
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  gsk_path_to_cairo (fill->path, cr);
  gdk_cairo_set_source_color (cr, GDK_COLOR_STATE_SRGB, &fill->color);
  cairo_fill (cr);
}

static void
gsk_gpu_node_processor_add_fill_node (GskGpuNodeProcessor *self,
                                      GskRenderNode       *node)
{
  graphene_rect_t clip_bounds, source_rect;
  GskGpuImage *mask_image, *source_image;
  GskRenderNode *child;
  GdkColor color;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip_bounds))
    return;
  gsk_rect_snap_to_grid (&clip_bounds, &self->scale, &self->offset, &clip_bounds);

  child = gsk_fill_node_get_child (node);

  if (GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE)
    gdk_color_init_copy (&color, gsk_color_node_get_color2 (child));
  else
    gdk_color_init (&color, GDK_COLOR_STATE_SRGB, (float[]) { 1, 1, 1, 1 });

  mask_image = gsk_gpu_upload_cairo_op (self->frame,
                                        &self->scale,
                                        &clip_bounds,
                                        gsk_gpu_node_processor_fill_path,
                                        g_memdup2 (&(FillData) {
                                            .path = gsk_path_ref (gsk_fill_node_get_path (node)),
                                            .color = color,
                                            .fill_rule = gsk_fill_node_get_fill_rule (node)
                                        }, sizeof (FillData)),
                                        (GDestroyNotify) gsk_fill_data_free);
  g_return_if_fail (mask_image != NULL);
  if (GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE)
    {
      gsk_gpu_node_processor_image_op (self,
                                       mask_image,
                                       GDK_COLOR_STATE_SRGB,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &clip_bounds,
                                       &clip_bounds);
      return;
    }

  source_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                           0,
                                                           &clip_bounds,
                                                           child,
                                                           &source_rect);
  if (source_image == NULL)
    return;

  gsk_gpu_mask_op (self->frame,
                   gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &clip_bounds),
                   &clip_bounds,
                   &self->offset,
                   self->opacity,
                   GSK_MASK_MODE_ALPHA,
                   &(GskGpuShaderImage) {
                       source_image,
                       GSK_GPU_SAMPLER_DEFAULT,
                       NULL,
                       &source_rect,
                   },
                   &(GskGpuShaderImage) {
                       mask_image,
                       GSK_GPU_SAMPLER_DEFAULT,
                       NULL,
                       &clip_bounds,
                   });

  g_object_unref (source_image);
}

typedef struct _StrokeData StrokeData;
struct _StrokeData
{
  GskPath *path;
  GdkColor color;
  GskStroke stroke;
};

static void
gsk_stroke_data_free (gpointer data)
{
  StrokeData *stroke = data;

  gdk_color_finish (&stroke->color);
  gsk_path_unref (stroke->path);
  gsk_stroke_clear (&stroke->stroke);
  g_free (stroke);
}

static void
gsk_gpu_node_processor_stroke_path (gpointer  data,
                                    cairo_t  *cr)
{
  StrokeData *stroke = data;

  gsk_stroke_to_cairo (&stroke->stroke, cr);
  gsk_path_to_cairo (stroke->path, cr);
  gdk_cairo_set_source_color (cr, GDK_COLOR_STATE_SRGB, &stroke->color);
  cairo_stroke (cr);
}

static void
gsk_gpu_node_processor_add_stroke_node (GskGpuNodeProcessor *self,
                                        GskRenderNode       *node)
{
  graphene_rect_t clip_bounds, source_rect;
  GskGpuImage *mask_image, *source_image;
  GskRenderNode *child;
  GdkColor color;

  if (!gsk_gpu_node_processor_clip_node_bounds (self, node, &clip_bounds))
    return;
  gsk_rect_snap_to_grid (&clip_bounds, &self->scale, &self->offset, &clip_bounds);

  child = gsk_stroke_node_get_child (node);

  if (GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE)
    gdk_color_init_copy (&color, gsk_color_node_get_color2 (child));
  else
    gdk_color_init (&color, GDK_COLOR_STATE_SRGB, (float[]) { 1, 1, 1, 1 });

  mask_image = gsk_gpu_upload_cairo_op (self->frame,
                                        &self->scale,
                                        &clip_bounds,
                                        gsk_gpu_node_processor_stroke_path,
                                        g_memdup2 (&(StrokeData) {
                                            .path = gsk_path_ref (gsk_stroke_node_get_path (node)),
                                            .color = color,
                                            .stroke = GSK_STROKE_INIT_COPY (gsk_stroke_node_get_stroke (node))
                                        }, sizeof (StrokeData)),
                                        (GDestroyNotify) gsk_stroke_data_free);
  g_return_if_fail (mask_image != NULL);
  if (GSK_RENDER_NODE_TYPE (child) == GSK_COLOR_NODE)
    {
      gsk_gpu_node_processor_image_op (self,
                                       mask_image,
                                       GDK_COLOR_STATE_SRGB,
                                       GSK_GPU_SAMPLER_DEFAULT,
                                       &clip_bounds,
                                       &clip_bounds);
      return;
    }

  source_image = gsk_gpu_node_processor_get_node_as_image (self,
                                                           0,
                                                           &clip_bounds,
                                                           child,
                                                           &source_rect);
  if (source_image == NULL)
    return;

  gsk_gpu_mask_op (self->frame,
                   gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &clip_bounds),
                   &clip_bounds,
                   &self->offset,
                   self->opacity,
                   GSK_MASK_MODE_ALPHA,
                   &(GskGpuShaderImage) {
                       source_image,
                       GSK_GPU_SAMPLER_DEFAULT,
                       NULL,
                       &source_rect,
                   },
                   &(GskGpuShaderImage) {
                       mask_image,
                       GSK_GPU_SAMPLER_DEFAULT,
                       NULL,
                       &clip_bounds,
                   });

  g_object_unref (source_image);
}

static void
gsk_gpu_node_processor_add_subsurface_node (GskGpuNodeProcessor *self,
                                            GskRenderNode       *node)
{
  GdkSubsurface *subsurface;

  subsurface = gsk_subsurface_node_get_subsurface (node);
  if (subsurface == NULL ||
      gdk_subsurface_get_texture (subsurface) == NULL ||
      gdk_subsurface_get_parent (subsurface) != gdk_draw_context_get_surface (gsk_gpu_frame_get_context (self->frame)))
    {
      gsk_gpu_node_processor_add_node (self, gsk_subsurface_node_get_child (node));
      return;
    }

  if (!gdk_subsurface_is_above_parent (subsurface))
    {
      cairo_rectangle_int_t int_clipped;
      graphene_rect_t rect, clipped;

      gsk_rect_init_offset (&rect, &node->bounds, &self->offset);
      gsk_rect_intersection (&self->clip.rect.bounds, &rect, &clipped);

      if (gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_CLEAR) &&
          node->bounds.size.width * node->bounds.size.height > 100 * 100 && /* not worth the effort for small images */
          (self->clip.type != GSK_GPU_CLIP_ROUNDED ||
           gsk_gpu_clip_contains_rect (&self->clip, &GRAPHENE_POINT_INIT(0,0), &clipped)) &&
          gsk_gpu_node_processor_rect_is_integer (self, &clipped, &int_clipped))
        {
          if (gdk_rectangle_intersect (&int_clipped, &self->scissor, &int_clipped))
            {
              float color[4] = { 0, 0, 0, 0 };
              gsk_gpu_clear_op (self->frame, &int_clipped, color);
            }
        }
      else
        {
          self->blend = GSK_GPU_BLEND_CLEAR;
          self->pending_globals |= GSK_GPU_GLOBAL_BLEND;
          gsk_gpu_node_processor_sync_globals (self, 0);
          GdkColor white;

          gdk_color_init (&white, self->ccs, ((float[]) { 1, 1, 1, 1 }));
          gsk_gpu_color_op (self->frame,
                            gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, &node->bounds),
                            self->ccs,
                            1,
                            &self->offset,
                            &node->bounds,
                            &white);
          gdk_color_finish (&white);

          self->blend = GSK_GPU_BLEND_OVER;
          self->pending_globals |= GSK_GPU_GLOBAL_BLEND;
        }
    }
}

static gboolean
gsk_gpu_node_processor_add_first_subsurface_node (GskGpuNodeProcessor *self,
                                                  GskGpuFirstNodeInfo *info,
                                                  GskRenderNode       *node)
{
  GdkSubsurface *subsurface;

  subsurface = gsk_subsurface_node_get_subsurface (node);
  if (subsurface == NULL ||
      gdk_subsurface_get_texture (subsurface) == NULL ||
      gdk_subsurface_get_parent (subsurface) != gdk_draw_context_get_surface (gsk_gpu_frame_get_context (self->frame)))
    {
      return gsk_gpu_node_processor_add_first_node (self,
                                                    info,
                                                    gsk_subsurface_node_get_child (node));
    }

  if (gdk_subsurface_is_above_parent (subsurface))
    return FALSE;

  if (!gsk_gpu_node_processor_clip_first_node (self, info, &node->bounds))
    return FALSE;

  gsk_gpu_first_node_begin_rendering (self, info, GSK_VEC4_TRANSPARENT);

  return TRUE;
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
#ifndef G_DISABLE_ASSERT
  GdkSubsurface *subsurface;

  subsurface = gsk_subsurface_node_get_subsurface (node);
  g_assert (subsurface == NULL ||
            gdk_subsurface_get_texture (subsurface) == NULL ||
            gdk_subsurface_get_parent (subsurface) != gdk_draw_context_get_surface (gsk_gpu_frame_get_context (frame)));
#endif

  return gsk_gpu_get_node_as_image (frame,
                                    flags,
                                    ccs,
                                    clip_bounds,
                                    scale,
                                    gsk_subsurface_node_get_child (node),
                                    out_bounds);
}

static void
gsk_gpu_node_processor_add_container_node (GskGpuNodeProcessor *self,
                                           GskRenderNode       *node)
{
  GskRenderNode **children;
  guint n_children;

  if (self->opacity < 1.0 && !gsk_container_node_is_disjoint (node))
    {
      gsk_gpu_node_processor_add_without_opacity (self, node);
      return;
    }

  children = gsk_container_node_get_children (node, &n_children);
  for (guint i = 0; i < n_children; i++)
    gsk_gpu_node_processor_add_node (self, children[i]);
}

static gboolean
gsk_gpu_node_processor_add_first_container_node (GskGpuNodeProcessor *self,
                                                 GskGpuFirstNodeInfo *info,
                                                 GskRenderNode       *node)
{
  GskRenderNode **children;
  graphene_rect_t opaque;
  int i;
  guint n;

  children = gsk_container_node_get_children (node, &n);
  if (n == 0)
    return FALSE;

  if (!gsk_render_node_get_opaque_rect (node, &opaque) ||
      !gsk_gpu_node_processor_clip_first_node (self, info, &opaque))
    return FALSE;

  for (i = n; i-->0; )
    {
      if (gsk_gpu_node_processor_add_first_node (self, info, children[i]))
        break;
    }

  if (i < 0)
    gsk_gpu_first_node_begin_rendering (self, info, NULL);

  for (i++; i < n; i++)
    gsk_gpu_node_processor_add_node (self, children[i]);

  return TRUE;
}

static void
gsk_gpu_node_processor_add_debug_node (GskGpuNodeProcessor *self,
                                       GskRenderNode       *node)
{
  gsk_gpu_node_processor_add_node (self, gsk_debug_node_get_child (node));
}

static gboolean
gsk_gpu_node_processor_add_first_debug_node (GskGpuNodeProcessor *self,
                                             GskGpuFirstNodeInfo *info,
                                             GskRenderNode       *node)
{
  return gsk_gpu_node_processor_add_first_node (self,
                                                info,
                                                gsk_debug_node_get_child (node));
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
  return gsk_gpu_get_node_as_image (frame,
                                    flags,
                                    ccs,
                                    clip_bounds,
                                    scale,
                                    gsk_debug_node_get_child (node),
                                    out_bounds);
}

typedef enum {
  GSK_GPU_HANDLE_OPACITY = (1 << 0)
} GskGpuNodeFeatures;

static const struct
{
  GskGpuGlobals ignored_globals;
  GskGpuNodeFeatures features;
  void                  (* process_node)                        (GskGpuNodeProcessor    *self,
                                                                 GskRenderNode          *node);
  gboolean              (* process_first_node)                  (GskGpuNodeProcessor    *self,
                                                                 GskGpuFirstNodeInfo    *info,
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
    0,
    0,
    NULL,
    NULL,
    NULL,
  },
  [GSK_CONTAINER_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_container_node,
    gsk_gpu_node_processor_add_first_container_node,
    NULL,
  },
  [GSK_CAIRO_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_cairo_node,
    NULL,
    gsk_gpu_get_cairo_node_as_image,
  },
  [GSK_COLOR_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_color_node,
    gsk_gpu_node_processor_add_first_color_node,
    NULL,
  },
  [GSK_LINEAR_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_linear_gradient_node,
    NULL,
    NULL,
  },
  [GSK_REPEATING_LINEAR_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_linear_gradient_node,
    NULL,
    NULL,
  },
  [GSK_RADIAL_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_radial_gradient_node,
    NULL,
    NULL,
  },
  [GSK_REPEATING_RADIAL_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_radial_gradient_node,
    NULL,
    NULL,
  },
  [GSK_CONIC_GRADIENT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_conic_gradient_node,
    NULL,
    NULL,
  },
  [GSK_BORDER_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_border_node,
    NULL,
    NULL,
  },
  [GSK_TEXTURE_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_texture_node,
    NULL,
    gsk_gpu_get_texture_node_as_image,
  },
  [GSK_INSET_SHADOW_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_inset_shadow_node,
    NULL,
    NULL,
  },
  [GSK_OUTSET_SHADOW_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_outset_shadow_node,
    NULL,
    NULL,
  },
  [GSK_TRANSFORM_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_transform_node,
    gsk_gpu_node_processor_add_first_transform_node,
    NULL,
  },
  [GSK_OPACITY_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_opacity_node,
    NULL,
    NULL,
  },
  [GSK_COLOR_MATRIX_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_color_matrix_node,
    NULL,
    NULL,
  },
  [GSK_REPEAT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_repeat_node,
    NULL,
    NULL,
  },
  [GSK_CLIP_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_clip_node,
    gsk_gpu_node_processor_add_first_clip_node,
    NULL,
  },
  [GSK_ROUNDED_CLIP_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_rounded_clip_node,
    gsk_gpu_node_processor_add_first_rounded_clip_node,
    NULL,
  },
  [GSK_SHADOW_NODE] = {
    0,
    0,
    gsk_gpu_node_processor_add_shadow_node,
    NULL,
    NULL,
  },
  [GSK_BLEND_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_blend_node,
    NULL,
    NULL,
  },
  [GSK_CROSS_FADE_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_cross_fade_node,
    NULL,
    NULL,
  },
  [GSK_TEXT_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_glyph_node,
    NULL,
    NULL,
  },
  [GSK_BLUR_NODE] = {
    0,
    0,
    gsk_gpu_node_processor_add_blur_node,
    NULL,
    NULL,
  },
  [GSK_DEBUG_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_debug_node,
    gsk_gpu_node_processor_add_first_debug_node,
    gsk_gpu_get_debug_node_as_image,
  },
  [GSK_GL_SHADER_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_gl_shader_node,
    NULL,
    NULL,
  },
  [GSK_TEXTURE_SCALE_NODE] = {
    0,
    0,
    gsk_gpu_node_processor_add_texture_scale_node,
    NULL,
    NULL,
  },
  [GSK_MASK_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_mask_node,
    NULL,
    NULL,
  },
  [GSK_FILL_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_fill_node,
    NULL,
    NULL,
  },
  [GSK_STROKE_NODE] = {
    0,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_stroke_node,
    NULL,
    NULL,
  },
  [GSK_SUBSURFACE_NODE] = {
    GSK_GPU_GLOBAL_MATRIX | GSK_GPU_GLOBAL_SCALE | GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR | GSK_GPU_GLOBAL_BLEND,
    GSK_GPU_HANDLE_OPACITY,
    gsk_gpu_node_processor_add_subsurface_node,
    gsk_gpu_node_processor_add_first_subsurface_node,
    gsk_gpu_get_subsurface_node_as_image,
  },
};

static void
gsk_gpu_node_processor_add_node (GskGpuNodeProcessor *self,
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

  if (self->opacity < 1.0 && (nodes_vtable[node_type].features & GSK_GPU_HANDLE_OPACITY) == 0)
    {
      gsk_gpu_node_processor_add_without_opacity (self, node);
      return;
    }

  gsk_gpu_node_processor_sync_globals (self, nodes_vtable[node_type].ignored_globals);
  g_assert ((self->pending_globals & ~nodes_vtable[node_type].ignored_globals) == 0);

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

static gboolean
gsk_gpu_node_processor_add_first_node (GskGpuNodeProcessor *self,
                                       GskGpuFirstNodeInfo *info,
                                       GskRenderNode       *node)
{
  GskRenderNodeType node_type;
  graphene_rect_t opaque;

  /* This catches the corner cases of empty nodes, so after this check
   * there's quaranteed to be at least 1 pixel that needs to be drawn
   */
  if (node->bounds.size.width == 0 || node->bounds.size.height == 0 ||
      !gsk_render_node_get_opaque_rect (node, &opaque))
    return FALSE;

  if (!gsk_gpu_clip_may_intersect_rect (&self->clip, &self->offset, &node->bounds))
    return FALSE;

  node_type = gsk_render_node_get_node_type (node);
  if (node_type >= G_N_ELEMENTS (nodes_vtable))
    {
      g_critical ("unknown node type %u for %s", node_type, g_type_name_from_instance ((GTypeInstance *) node));
      return FALSE;
    }

  if (nodes_vtable[node_type].process_first_node)
    return nodes_vtable[node_type].process_first_node (self, info, node);

  /* fallback starts here */
  if (!gsk_gpu_node_processor_clip_first_node (self, info, &opaque))
    return FALSE;

  gsk_gpu_first_node_begin_rendering (self, info, NULL);
  gsk_gpu_node_processor_add_node (self, node);

  return TRUE;
}

/*
 * gsk_gpu_get_node_as_image:
 * @frame: frame to render in
 * @flags: flags for the image
 * @ccs: the color state to composite the image in
 * @clip_bounds: region of node that must be included in image
 * @scale: scale factor to use for the image
 * @node: the node to render
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
gsk_gpu_node_processor_set_scissor (GskGpuNodeProcessor         *self,
                                    const cairo_rectangle_int_t *scissor)
{
  graphene_rect_t clip_rect;

  self->scissor = *scissor;

  if (!gsk_gpu_node_processor_rect_device_to_clip (self,
                                                   &GSK_RECT_INIT_CAIRO (scissor),
                                                   &clip_rect))
    {
      /* If the transform is screwed up in the places where we call this function,
       * something has seriously gone wrong.
       */
      g_assert_not_reached ();
    }
  
  gsk_gpu_clip_init_empty (&self->clip, &clip_rect);

  self->pending_globals |= GSK_GPU_GLOBAL_CLIP | GSK_GPU_GLOBAL_SCISSOR;
}

static void
gsk_gpu_node_processor_render (GskGpuNodeProcessor   *self,
                               GskGpuImage           *target,
                               cairo_region_t        *clip,
                               GskRenderNode         *node,
                               GskRenderPassType      pass_type)
{
  GskGpuFirstNodeInfo info = {
    .target = target,
    .pass_type = pass_type,
  };
  int i, n, best, best_size;
  cairo_rectangle_int_t rect;
  gboolean do_culling;

  cairo_region_get_extents (clip, &info.extents);
  info.whole_area = cairo_region_contains_rectangle (clip, &info.extents) == CAIRO_REGION_OVERLAP_IN;
  info.min_occlusion_pixels = gsk_gpu_image_get_width (target) * gsk_gpu_image_get_height (target) *
                              MIN_PERCENTAGE_FOR_OCCLUSION_PASS / 100;
  info.min_occlusion_pixels = MAX (info.min_occlusion_pixels, MIN_PIXELS_FOR_OCCLUSION_PASS);

  do_culling = gsk_gpu_frame_should_optimize (self->frame, GSK_GPU_OPTIMIZE_OCCLUSION_CULLING);

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

      cairo_region_get_rectangle (clip, best, &rect);

      if (best_size < MIN_PIXELS_FOR_OCCLUSION_PASS)
        break;

      gsk_gpu_node_processor_set_scissor (self, &rect);

      if (!gsk_gpu_node_processor_add_first_node (self,
                                                  &info,
                                                  node))
        {
          gsk_gpu_first_node_begin_rendering (self, &info, GSK_VEC4_TRANSPARENT);
          gsk_gpu_node_processor_add_node (self, node);
          do_culling = FALSE;
        }
      else if (GSK_DEBUG_CHECK (OCCLUSION))
        {
          gsk_gpu_node_processor_sync_globals (self, 0);
          gsk_gpu_color_op (self->frame,
                            GSK_GPU_SHADER_CLIP_NONE,
                            self->ccs,
                            1.0,
                            &self->offset,
                            &GRAPHENE_RECT_INIT(0, 0, 10000, 10000),
                            &GDK_COLOR_SRGB (1.0, 1.0, 1.0, 0.6));
        }

      cairo_region_subtract_rectangle (clip, &self->scissor);
    }

  for (i = 0; i < cairo_region_num_rectangles (clip); i++) 
    {
      cairo_region_get_rectangle (clip, i, &rect);

      gsk_gpu_node_processor_set_scissor (self, &rect);

      /* only run pass if it's covering the whole rectangle */
      info.min_occlusion_pixels = rect.width * rect.height;

      if (!gsk_gpu_node_processor_add_first_node (self,
                                                  &info,
                                                  node))
        {
          gsk_gpu_first_node_begin_rendering (self, &info, GSK_VEC4_TRANSPARENT);
          gsk_gpu_node_processor_add_node (self, node);
        }
    }

  if (info.has_started_rendering)
    {
      gsk_gpu_render_pass_end_op (self->frame,
                                  target,
                                  pass_type);
    }
}

static void
gsk_gpu_node_processor_convert_to (GskGpuNodeProcessor   *self,
                                   GskGpuImage           *image,
                                   GdkColorState         *image_color_state,
                                   const graphene_rect_t *rect,
                                   const graphene_rect_t *tex_rect)
{
  gsk_gpu_node_processor_sync_globals (self, 0);

  if (!GDK_IS_DEFAULT_COLOR_STATE (self->ccs))
    {
      const GdkCicp *cicp = gdk_color_state_get_cicp (self->ccs);

      g_assert (cicp != NULL);

      gsk_gpu_convert_to_cicp_op (self->frame,
                                  gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                                  cicp,
                                  gsk_gpu_color_states_create_cicp (image_color_state, TRUE, TRUE),
                                  self->opacity,
                                  FALSE,
                                  &self->offset,
                                  &(GskGpuShaderImage) {
                                      image,
                                      GSK_GPU_SAMPLER_DEFAULT,
                                      rect,
                                      tex_rect
                                  });
    }
  else
    {
      gsk_gpu_convert_op (self->frame,
                          gsk_gpu_clip_get_shader_clip (&self->clip, &self->offset, rect),
                          gsk_gpu_node_processor_color_states_explicit (self, image_color_state, TRUE),
                          self->opacity,
                          FALSE,
                          &self->offset,
                          &(GskGpuShaderImage) {
                              image,
                              GSK_GPU_SAMPLER_DEFAULT,
                              rect,
                              tex_rect
                          });
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
  GskGpuNodeProcessor self;
  GdkColorState *ccs;
  GskGpuImage *image;
  graphene_rect_t clip_bounds, tex_rect;
  cairo_rectangle_int_t extents;
  int i;

  ccs = gdk_color_state_get_rendering_color_state (target_color_state);

  cairo_region_get_extents (clip, &extents);

  gsk_gpu_node_processor_init (&self,
                               frame,
                               target,
                               target_color_state,
                               &extents,
                               viewport);

  if (gdk_color_state_equal (ccs, target_color_state))
    {
      gsk_gpu_node_processor_render (&self, target, clip, node, pass_type);
    }
  else
    {
      for (i = 0; i < cairo_region_num_rectangles (clip); i++)
        {
          cairo_rectangle_int_t rect;

          cairo_region_get_rectangle (clip, i, &rect);
          gsk_gpu_node_processor_set_scissor (&self, &rect);

          /* Can't use gsk_gpu_node_processor_get_node_as_image () because of colorspaces */
          if (!gsk_gpu_node_processor_clip_node_bounds (&self, node, &clip_bounds))
            continue;

          gsk_rect_snap_to_grid (&clip_bounds, &self.scale, &self.offset, &clip_bounds);
          image = gsk_gpu_get_node_as_image (self.frame,
                                             0,
                                             ccs,
                                             &clip_bounds,
                                             &self.scale,
                                             node,
                                             &tex_rect);
          if (image == NULL)
            continue;

          gsk_gpu_render_pass_begin_op (frame,
                                        target,
                                        &rect,
                                        GSK_GPU_LOAD_OP_DONT_CARE,
                                        NULL,
                                        pass_type);

          self.blend = GSK_GPU_BLEND_NONE;
          self.pending_globals |= GSK_GPU_GLOBAL_BLEND;

          gsk_gpu_node_processor_convert_to (&self,
                                             image,
                                             ccs,
                                             &clip_bounds,
                                             &tex_rect);

          gsk_gpu_render_pass_end_op (frame,
                                      target,
                                      pass_type);

          g_object_unref (image);
        }
    }

  gsk_gpu_node_processor_finish (&self);

  cairo_region_destroy (clip);
}

GskGpuImage *
gsk_gpu_node_processor_convert_image (GskGpuFrame     *frame,
                                      GdkMemoryFormat  target_format,
                                      GdkColorState   *target_color_state,
                                      GskGpuImage     *image,
                                      GdkColorState   *image_color_state)
{
  GskGpuNodeProcessor self;
  GskGpuImage *converted, *intermediate = NULL;
  gsize width, height;

  width = gsk_gpu_image_get_width (image);
  height = gsk_gpu_image_get_height (image);

  converted = gsk_gpu_device_create_offscreen_image (gsk_gpu_frame_get_device (frame),
                                                     FALSE,
                                                     target_format,
                                                     gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_SRGB,
                                                     width,
                                                     height);
  if (converted == NULL)
    return NULL;

  /* We need to go via an intermediate colorstate */
  if (!GDK_IS_DEFAULT_COLOR_STATE (image_color_state) &&
      !GDK_IS_DEFAULT_COLOR_STATE (target_color_state))
    {
      GdkColorState *ccs = gdk_color_state_get_rendering_color_state (image_color_state);

      intermediate = gsk_gpu_copy_image (frame, ccs, image, image_color_state, FALSE);
      if (intermediate == NULL)
        return NULL;
      image = intermediate;
      image_color_state = ccs;
    }

  gsk_gpu_node_processor_init (&self,
                               frame,
                               converted,
                               target_color_state,
                               &(cairo_rectangle_int_t) { 0, 0, width, height },
                               &GRAPHENE_RECT_INIT (0, 0, width, height));
  gsk_gpu_render_pass_begin_op (frame,
                                converted,
                                &(cairo_rectangle_int_t) { 0, 0, width, height },
                                GSK_GPU_LOAD_OP_DONT_CARE,
                                NULL,
                                GSK_RENDER_PASS_OFFSCREEN);

  gsk_gpu_node_processor_sync_globals (&self, 0);

  if (GDK_IS_DEFAULT_COLOR_STATE (target_color_state))
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
                                         image,
                                         image_color_state,
                                         &GRAPHENE_RECT_INIT (0, 0, width, height),
                                         &GRAPHENE_RECT_INIT (0, 0, width, height));
    }

  gsk_gpu_render_pass_end_op (self.frame,
                              converted,
                              GSK_RENDER_PASS_OFFSCREEN);
  gsk_gpu_node_processor_finish (&self);

  g_clear_object (&intermediate);

  return converted;
}

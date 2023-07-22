#include "config.h"

#include "gskvulkanrenderpassprivate.h"

#include "gskdebugprivate.h"
#include "gskprofilerprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderer.h"
#include "gskrendererprivate.h"
#include "gskroundedrectprivate.h"
#include "gsktransform.h"
#include "gskvulkanblendmodeopprivate.h"
#include "gskvulkanbluropprivate.h"
#include "gskvulkanborderopprivate.h"
#include "gskvulkanclearopprivate.h"
#include "gskvulkanclipprivate.h"
#include "gskvulkancolormatrixopprivate.h"
#include "gskvulkancoloropprivate.h"
#include "gskvulkanconvertopprivate.h"
#include "gskvulkancrossfadeopprivate.h"
#include "gskvulkanglyphopprivate.h"
#include "gskvulkaninsetshadowopprivate.h"
#include "gskvulkanlineargradientopprivate.h"
#include "gskvulkanmaskopprivate.h"
#include "gskvulkanopprivate.h"
#include "gskvulkanprivate.h"
#include "gskvulkanrendererprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanrenderpassopprivate.h"
#include "gskvulkanoutsetshadowopprivate.h"
#include "gskvulkanpushconstantsopprivate.h"
#include "gskvulkanscissoropprivate.h"
#include "gskvulkantextureopprivate.h"
#include "gskvulkanuploadopprivate.h"
#include "gskprivate.h"

#include "gdk/gdkvulkancontextprivate.h"

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

typedef struct _GskVulkanParseState GskVulkanParseState;

struct _GskVulkanRenderPass
{
  int empty;
};

struct _GskVulkanParseState
{
  cairo_rectangle_int_t  scissor;
  graphene_point_t       offset;
  graphene_vec2_t        scale;
  GskTransform          *modelview;
  graphene_matrix_t      projection;
  GskVulkanClip          clip;
};

GskVulkanRenderPass *
gsk_vulkan_render_pass_new (void)
{
  GskVulkanRenderPass *self;

  self = g_new0 (GskVulkanRenderPass, 1);

  return self;
}

void
gsk_vulkan_render_pass_free (GskVulkanRenderPass *self)
{
  g_free (self);
}

static void
gsk_vulkan_render_pass_append_scissor (GskVulkanRender           *render,
                                       GskRenderNode             *node,
                                       const GskVulkanParseState *state)
{
  gsk_vulkan_scissor_op (render, &state->scissor);
}

static void
gsk_vulkan_render_pass_append_push_constants (GskVulkanRender           *render,
                                              GskRenderNode             *node,
                                              const GskVulkanParseState *state)
{
  graphene_matrix_t mvp;

  if (state->modelview)
    {
      gsk_transform_to_matrix (state->modelview, &mvp);
      graphene_matrix_multiply (&mvp, &state->projection, &mvp);
    }
  else
    graphene_matrix_init_from_matrix (&mvp, &state->projection);

  gsk_vulkan_push_constants_op (render, &state->scale, &mvp, &state->clip.rect);
}

#define FALLBACK(...) G_STMT_START { \
  GSK_RENDERER_DEBUG (gsk_vulkan_render_get_renderer (render), FALLBACK, __VA_ARGS__); \
  return FALSE; \
}G_STMT_END

static gboolean
gsk_vulkan_parse_rect_is_integer (const GskVulkanParseState *state,
                                  const graphene_rect_t     *rect,
                                  cairo_rectangle_int_t     *int_rect)
{
  graphene_rect_t transformed_rect;
  float scale_x = graphene_vec2_get_x (&state->scale);
  float scale_y = graphene_vec2_get_y (&state->scale);

  switch (gsk_transform_get_category (state->modelview))
    {
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
      return FALSE;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      gsk_transform_transform_bounds (state->modelview, rect, &transformed_rect);
      rect = &transformed_rect;
      break;

    case GSK_TRANSFORM_CATEGORY_IDENTITY:
    default:
      break;
    } 
  int_rect->x = rect->origin.x * scale_x;
  int_rect->y = rect->origin.y * scale_y;
  int_rect->width = rect->size.width * scale_x;
  int_rect->height = rect->size.height * scale_y;

  return int_rect->x == rect->origin.x * scale_x
      && int_rect->y == rect->origin.y * scale_y
      && int_rect->width == rect->size.width * scale_x
      && int_rect->height == rect->size.height * scale_y;
}

static GskVulkanImage *
gsk_vulkan_render_pass_upload_texture (GskVulkanRender *render,
                                       GdkTexture      *texture)
{
  GskVulkanImage *image, *better_image;
  int width, height;
  GskVulkanImagePostprocess postproc;
  graphene_matrix_t projection;
  graphene_vec2_t scale;

  image = gsk_vulkan_upload_texture_op (render, texture);
  postproc = gsk_vulkan_image_get_postprocess (image);
  if (postproc == 0)
    return image;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  better_image = gsk_vulkan_image_new_for_offscreen (gsk_vulkan_render_get_context (render),
                                                     gdk_texture_get_format (texture),
                                                     width, height);
  gsk_vulkan_render_pass_begin_op (render,
                                   better_image,
                                   &(cairo_rectangle_int_t) { 0, 0, width, height },
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  gsk_vulkan_scissor_op (render, &(cairo_rectangle_int_t) { 0, 0, width, height });
  graphene_matrix_init_ortho (&projection,
                              0, width,
                              0, height,
                              2 * ORTHO_NEAR_PLANE - ORTHO_FAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_vec2_init (&scale, 1.0, 1.0);
  gsk_vulkan_push_constants_op (render, &scale, &projection, &GSK_ROUNDED_RECT_INIT(0, 0, width, height));
  gsk_vulkan_convert_op (render,
                         GSK_VULKAN_SHADER_CLIP_NONE,
                         image,
                         &GRAPHENE_RECT_INIT (0, 0, width, height),
                         &GRAPHENE_POINT_INIT (0, 0),
                         &GRAPHENE_RECT_INIT (0, 0, width, height));
  gsk_vulkan_render_pass_end_op (render,
                                 better_image,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  g_object_unref (better_image);

  return better_image;
}

static GskVulkanImage *
gsk_vulkan_render_pass_get_node_as_image (GskVulkanRenderPass       *self,
                                          GskVulkanRender           *render,
                                          const GskVulkanParseState *state,
                                          GskRenderNode             *node,
                                          graphene_rect_t           *tex_bounds)
{
  GskVulkanImage *result;

  switch ((guint) gsk_render_node_get_node_type (node))
    {
    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        GskVulkanRenderer *renderer = GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render));
        result = gsk_vulkan_renderer_get_texture_image (renderer, texture);
        if (result == NULL)
          {
            result = gsk_vulkan_render_pass_upload_texture (render, texture);
            gsk_vulkan_renderer_add_texture_image (renderer, texture, result);
          }

        *tex_bounds = node->bounds;
        return result;
      }

    case GSK_CAIRO_NODE:
      {
        graphene_rect_t clipped;

        graphene_rect_offset_r (&state->clip.rect.bounds, - state->offset.x, - state->offset.y, &clipped);
        graphene_rect_intersection (&clipped, &node->bounds, &clipped);

        if (clipped.size.width == 0 || clipped.size.height == 0)
          return NULL;

        result = gsk_vulkan_upload_cairo_op (render,
                                             node,
                                             &state->scale,
                                             &clipped);

        *tex_bounds = clipped;
        return result;
      }

    default:
      {
        graphene_rect_t clipped;

        graphene_rect_offset_r (&state->clip.rect.bounds, - state->offset.x, - state->offset.y, &clipped);
        graphene_rect_intersection (&clipped, &node->bounds, &clipped);

        if (clipped.size.width == 0 || clipped.size.height == 0)
          return NULL;

        /* assuming the unclipped bounds should go to texture coordinates 0..1,
         * calculate the coordinates for the clipped texture size
         */
        *tex_bounds = clipped;

        result = gsk_vulkan_render_pass_op_offscreen (render,
                                                      &state->scale,
                                                      &clipped,
                                                      node);

        return result;
      }
   }
}

static void
gsk_vulkan_render_pass_add_node (GskVulkanRenderPass       *self,
                                 GskVulkanRender           *render,
                                 const GskVulkanParseState *state,
                                 GskRenderNode             *node);

static inline gboolean
gsk_vulkan_render_pass_add_fallback_node (GskVulkanRenderPass       *self,
                                          GskVulkanRender           *render,
                                          const GskVulkanParseState *state,
                                          GskRenderNode             *node)
{
  GskVulkanImage *image;
  graphene_rect_t clipped;

  graphene_rect_offset_r (&state->clip.rect.bounds, - state->offset.x, - state->offset.y, &clipped);
  graphene_rect_intersection (&clipped, &node->bounds, &clipped);

  if (clipped.size.width == 0 || clipped.size.height == 0)
    return TRUE;

  image = gsk_vulkan_upload_cairo_op (render,
                                      node,
                                      &state->scale,
                                      &clipped);

  gsk_vulkan_texture_op (render,
                         gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                         image,
                         GSK_VULKAN_SAMPLER_DEFAULT,
                         &node->bounds,
                         &state->offset,
                         &clipped);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_implode (GskVulkanRenderPass       *self,
                                GskVulkanRender           *render,
                                const GskVulkanParseState *state,
                                GskRenderNode             *node)
{
  g_assert_not_reached ();
  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_container_node (GskVulkanRenderPass       *self,
                                           GskVulkanRender           *render,
                                           const GskVulkanParseState *state,
                                           GskRenderNode             *node)
{
  for (guint i = 0; i < gsk_container_node_get_n_children (node); i++)
    gsk_vulkan_render_pass_add_node (self, render, state, gsk_container_node_get_child (node, i));

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_cairo_node (GskVulkanRenderPass       *self,
                                       GskVulkanRender           *render,
                                       const GskVulkanParseState *state,
                                       GskRenderNode             *node)
{
  /* We're using recording surfaces, so drawing them to an image
   * surface and uploading them is the right thing.
   * But that's exactly what the fallback code does.
   */
  if (gsk_cairo_node_get_surface (node) != NULL)
    return gsk_vulkan_render_pass_add_fallback_node (self, render, state, node);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_color_node (GskVulkanRenderPass       *self,
                                       GskVulkanRender           *render,
                                       const GskVulkanParseState *state,
                                       GskRenderNode             *node)
{
  cairo_rectangle_int_t int_clipped;
  graphene_rect_t rect, clipped;
  const GdkRGBA *color;

  color = gsk_color_node_get_color (node);
  graphene_rect_offset_r (&node->bounds,
                          state->offset.x, state->offset.y,
                          &rect);
  graphene_rect_intersection (&state->clip.rect.bounds, &rect, &clipped);

  if (gdk_rgba_is_opaque (color) &&
      node->bounds.size.width * node->bounds.size.height > 100 * 100 && /* not worth the effort for small images */
      gsk_vulkan_parse_rect_is_integer (state, &clipped, &int_clipped))
    {
      /* now handle all the clip */
      if (!gdk_rectangle_intersect (&int_clipped, &state->scissor, &int_clipped))
        return TRUE;

      /* we have handled the bounds, now do the corners */
      if (state->clip.type == GSK_VULKAN_CLIP_ROUNDED)
        {
          graphene_rect_t cover;
          GskVulkanShaderClip shader_clip;
          float scale_x = graphene_vec2_get_x (&state->scale);
          float scale_y = graphene_vec2_get_y (&state->scale);
          clipped = GRAPHENE_RECT_INIT (int_clipped.x / scale_x, int_clipped.y / scale_y,
                                        int_clipped.width / scale_x, int_clipped.height / scale_y);
          shader_clip = gsk_vulkan_clip_get_shader_clip (&state->clip, graphene_point_zero(), &clipped);
          if (shader_clip != GSK_VULKAN_SHADER_CLIP_NONE)
            {
              gsk_rounded_rect_get_largest_cover (&state->clip.rect, &clipped, &cover);
              int_clipped.x = ceil (cover.origin.x * scale_x);
              int_clipped.y = ceil (cover.origin.y * scale_y);
              int_clipped.width = floor ((cover.origin.x + cover.size.width) * scale_x) - int_clipped.x;
              int_clipped.height = floor ((cover.origin.y + cover.size.height) * scale_y) - int_clipped.y;
              if (int_clipped.width == 0 || int_clipped.height == 0)
                {
                  gsk_vulkan_color_op (render,
                                       shader_clip,
                                       &clipped,
                                       graphene_point_zero (),
                                       color);
                  return TRUE;
                }
              cover = GRAPHENE_RECT_INIT (int_clipped.x / scale_x, int_clipped.y / scale_y,
                                          int_clipped.width / scale_x, int_clipped.height / scale_y);
              if (clipped.origin.x != cover.origin.x)
                gsk_vulkan_color_op (render,
                                     shader_clip,
                                     &GRAPHENE_RECT_INIT (clipped.origin.x, clipped.origin.y, cover.origin.x - clipped.origin.x, clipped.size.height),
                                     graphene_point_zero (),
                                     color);
              if (clipped.origin.y != cover.origin.y)
                gsk_vulkan_color_op (render,
                                     shader_clip,
                                     &GRAPHENE_RECT_INIT (clipped.origin.x, clipped.origin.y, clipped.size.width, cover.origin.y - clipped.origin.y),
                                     graphene_point_zero (),
                                     color);
              if (clipped.origin.x + clipped.size.width != cover.origin.x + cover.size.width)
                gsk_vulkan_color_op (render,
                                     shader_clip,
                                     &GRAPHENE_RECT_INIT (cover.origin.x + cover.size.width,
                                                          clipped.origin.y,
                                                          clipped.origin.x + clipped.size.width - cover.origin.x - cover.size.width,
                                                          clipped.size.height),
                                     graphene_point_zero (),
                                     color);
              if (clipped.origin.y + clipped.size.height != cover.origin.y + cover.size.height)
                gsk_vulkan_color_op (render,
                                     shader_clip,
                                     &GRAPHENE_RECT_INIT (clipped.origin.x,
                                                          cover.origin.y + cover.size.height,
                                                          clipped.size.width,
                                                          clipped.origin.y + clipped.size.height - cover.origin.y - cover.size.height),
                                     graphene_point_zero (),
                                     color);
            }
        }

      gsk_vulkan_clear_op (render,
                           &int_clipped,
                           color);
      return TRUE;
    }

  gsk_vulkan_color_op (render,
                       gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                       &rect,
                       graphene_point_zero (),
                       color);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_linear_gradient_node (GskVulkanRenderPass       *self,
                                                 GskVulkanRender           *render,
                                                 const GskVulkanParseState *state,
                                                 GskRenderNode             *node)
{
  gsk_vulkan_linear_gradient_op (render,
                                 gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                                 &node->bounds,
                                 &state->offset,
                                 gsk_linear_gradient_node_get_start (node),
                                 gsk_linear_gradient_node_get_end (node),
                                 gsk_render_node_get_node_type (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE,
                                 gsk_linear_gradient_node_get_color_stops (node, NULL),
                                 gsk_linear_gradient_node_get_n_color_stops (node));
  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_border_node (GskVulkanRenderPass       *self,
                                        GskVulkanRender           *render,
                                        const GskVulkanParseState *state,
                                        GskRenderNode             *node)
{
  gsk_vulkan_border_op (render,
                        gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                        gsk_border_node_get_outline (node),
                        &state->offset,
                        gsk_border_node_get_widths (node),
                        gsk_border_node_get_colors (node));
  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_texture_node (GskVulkanRenderPass       *self,
                                         GskVulkanRender           *render,
                                         const GskVulkanParseState *state,
                                         GskRenderNode             *node)
{
  GskVulkanImage *image;
  GskVulkanRenderer *renderer;
  GdkTexture *texture;

  renderer = GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render));
  texture = gsk_texture_node_get_texture (node);
  image = gsk_vulkan_renderer_get_texture_image (renderer, texture);
  if (image == NULL)
    {
      image = gsk_vulkan_render_pass_upload_texture (render, texture);
      gsk_vulkan_renderer_add_texture_image (renderer, texture, image);
    }

  gsk_vulkan_texture_op (render,
                         gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                         image,
                         GSK_VULKAN_SAMPLER_DEFAULT,
                         &node->bounds,
                         &state->offset,
                         &node->bounds);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_texture_scale_node (GskVulkanRenderPass       *self,
                                               GskVulkanRender           *render,
                                               const GskVulkanParseState *state,
                                               GskRenderNode             *node)
{
  GskVulkanImage *image;
  GskVulkanRenderer *renderer;
  GskVulkanRenderSampler sampler;
  GdkTexture *texture;

  renderer = GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render));
  texture = gsk_texture_scale_node_get_texture (node);
  switch (gsk_texture_scale_node_get_filter (node))
    {
    default:
      g_assert_not_reached ();
    case GSK_SCALING_FILTER_LINEAR:
    case GSK_SCALING_FILTER_TRILINEAR:
      sampler = GSK_VULKAN_SAMPLER_DEFAULT;
      break;
    case GSK_SCALING_FILTER_NEAREST:
      sampler = GSK_VULKAN_SAMPLER_NEAREST;
      break;
    }
  image = gsk_vulkan_renderer_get_texture_image (renderer, texture);
  if (image == NULL)
    {
      image = gsk_vulkan_render_pass_upload_texture (render, texture);
      gsk_vulkan_renderer_add_texture_image (renderer, texture, image);
    }

  gsk_vulkan_texture_op (render,
                         gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                         image,
                         sampler,
                         &node->bounds,
                         &state->offset,
                         &node->bounds);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_inset_shadow_node (GskVulkanRenderPass       *self,
                                              GskVulkanRender           *render,
                                              const GskVulkanParseState *state,
                                              GskRenderNode             *node)
{
  if (gsk_inset_shadow_node_get_blur_radius (node) > 0)
    FALLBACK ("Blur support not implemented for inset shadows");

  gsk_vulkan_inset_shadow_op (render,
                              gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                              gsk_inset_shadow_node_get_outline (node),
                              &state->offset,
                              gsk_inset_shadow_node_get_color (node),
                              &GRAPHENE_POINT_INIT (gsk_inset_shadow_node_get_dx (node),
                                                    gsk_inset_shadow_node_get_dy (node)),
                              gsk_inset_shadow_node_get_spread (node),
                              gsk_inset_shadow_node_get_blur_radius (node));

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_outset_shadow_node (GskVulkanRenderPass       *self,
                                               GskVulkanRender           *render,
                                               const GskVulkanParseState *state,
                                               GskRenderNode             *node)
{
  if (gsk_outset_shadow_node_get_blur_radius (node) > 0)
    FALLBACK ("Blur support not implemented for outset shadows");

  gsk_vulkan_outset_shadow_op (render,
                               gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                               gsk_outset_shadow_node_get_outline (node),
                               &state->offset,
                               gsk_outset_shadow_node_get_color (node),
                               &GRAPHENE_POINT_INIT (gsk_outset_shadow_node_get_dx (node),
                                                     gsk_outset_shadow_node_get_dy (node)),
                               gsk_outset_shadow_node_get_spread (node),
                               gsk_outset_shadow_node_get_blur_radius (node));

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_transform_node (GskVulkanRenderPass       *self,
                                           GskVulkanRender           *render,
                                           const GskVulkanParseState *state,
                                           GskRenderNode             *node)
{
  GskRenderNode *child;
  GskTransform *transform;
  GskVulkanParseState new_state;

  child = gsk_transform_node_get_child (node);
  transform = gsk_transform_node_get_transform (node);

  switch (gsk_transform_get_category (transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;
        gsk_transform_to_translate (transform, &dx, &dy);
        new_state = *state;
        new_state.offset.x += dx;
        new_state.offset.y += dy;
        gsk_vulkan_render_pass_add_node (self, render, &new_state, child);
      }
      return TRUE;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float dx, dy, scale_x, scale_y;
        gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
        gsk_vulkan_clip_scale (&new_state.clip, &state->clip, scale_x, scale_y);
        new_state.offset.x = (state->offset.x + dx) / scale_x;
        new_state.offset.y = (state->offset.y + dy) / scale_y;
        graphene_vec2_init (&new_state.scale, fabs (scale_x), fabs (scale_y));
        graphene_vec2_multiply (&new_state.scale, &state->scale, &new_state.scale);
        new_state.modelview = gsk_transform_scale (gsk_transform_ref (state->modelview),
                                                   scale_x / fabs (scale_x),
                                                   scale_y / fabs (scale_y));
      }
      break;

    case GSK_TRANSFORM_CATEGORY_2D:
      {
        float skew_x, skew_y, scale_x, scale_y, angle, dx, dy;
        GskTransform *clip_transform;

        clip_transform = gsk_transform_transform (gsk_transform_translate (NULL, &state->offset), transform);

        if (gsk_vulkan_clip_contains_rect (&state->clip, &state->offset, &node->bounds))
          {
            gsk_vulkan_clip_init_empty (&new_state.clip, &child->bounds);
          }
        else if (!gsk_vulkan_clip_transform (&new_state.clip, &state->clip, clip_transform, &child->bounds))
          {
            gsk_transform_unref (clip_transform);
            FALLBACK ("Transform nodes can't deal with clip type %u", state->clip.type);
          }

        new_state.modelview = gsk_transform_ref (state->modelview);
        new_state.modelview = gsk_transform_scale (state->modelview,
                                                   graphene_vec2_get_x (&state->scale),
                                                   graphene_vec2_get_y (&state->scale));
        new_state.modelview = gsk_transform_transform (new_state.modelview, clip_transform);
        gsk_transform_unref (clip_transform);

        gsk_transform_to_2d_components (new_state.modelview,
                                        &skew_x, &skew_y,
                                        &scale_x, &scale_y,
                                        &angle,
                                        &dx, &dy);
        scale_x = fabs (scale_x);
        scale_y = fabs (scale_y);
        new_state.modelview = gsk_transform_scale (new_state.modelview, 1 / scale_x, 1 / scale_y);
        graphene_vec2_init (&new_state.scale, scale_x, scale_y);
        new_state.offset = *graphene_point_zero ();
      }
      break;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
      {
        graphene_quaternion_t rotation;
        graphene_matrix_t matrix;
        graphene_vec4_t perspective;
        graphene_vec3_t translation;
        graphene_vec3_t matrix_scale;
        graphene_vec3_t shear;
        GskTransform *clip_transform;
        float scale_x, scale_y, old_pixels, new_pixels;

        clip_transform = gsk_transform_transform (gsk_transform_translate (NULL, &state->offset), transform);

        if (gsk_vulkan_clip_contains_rect (&state->clip, &state->offset, &node->bounds))
          {
            gsk_vulkan_clip_init_empty (&new_state.clip, &child->bounds);
          }
        else if (!gsk_vulkan_clip_transform (&new_state.clip, &state->clip, clip_transform, &child->bounds))
          {
            gsk_transform_unref (clip_transform);
            FALLBACK ("Transform nodes can't deal with clip type %u", state->clip.type);
          }

        new_state.modelview = gsk_transform_ref (state->modelview);
        new_state.modelview = gsk_transform_scale (state->modelview,
                                                   graphene_vec2_get_x (&state->scale),
                                                   graphene_vec2_get_y (&state->scale));                                                  
        new_state.modelview = gsk_transform_transform (new_state.modelview, clip_transform);
        gsk_transform_unref (clip_transform);

        gsk_transform_to_matrix (new_state.modelview, &matrix);
        graphene_matrix_decompose (&matrix,
                                   &translation,
                                   &matrix_scale,
                                   &rotation,
                                   &shear,
                                   &perspective);

        scale_x = fabs (graphene_vec3_get_x (&matrix_scale));
        scale_y = fabs (graphene_vec3_get_y (&matrix_scale));
        old_pixels = graphene_vec2_get_x (&state->scale) * graphene_vec2_get_y (&state->scale) *
                     state->clip.rect.bounds.size.width * state->clip.rect.bounds.size.height;
        new_pixels = scale_x * scale_y * new_state.clip.rect.bounds.size.width * new_state.clip.rect.bounds.size.height;
        if (new_pixels > 2 * old_pixels)
          {
            float forced_downscale = 2 * old_pixels / new_pixels;
            scale_x *= forced_downscale;
            scale_y *= forced_downscale;
          }
        new_state.modelview = gsk_transform_scale (new_state.modelview, 1 / scale_x, 1 / scale_y);
        graphene_vec2_init (&new_state.scale, scale_x, scale_y);
        new_state.offset = *graphene_point_zero ();
      }
      break;

    default:
      break;
    }

  new_state.scissor = state->scissor;
  graphene_matrix_init_from_matrix (&new_state.projection, &state->projection);

  gsk_vulkan_render_pass_append_push_constants (render, node, &new_state);

  gsk_vulkan_render_pass_add_node (self, render, &new_state, child);

  gsk_vulkan_render_pass_append_push_constants (render, node, state);

  gsk_transform_unref (new_state.modelview);

  return TRUE;
}

static gboolean
gsk_vulkan_render_pass_add_opacity_node (GskVulkanRenderPass       *self,
                                         GskVulkanRender           *render,
                                         const GskVulkanParseState *state,
                                         GskRenderNode             *node)
{
  GskVulkanImage *image;
  graphene_rect_t tex_rect;

  image = gsk_vulkan_render_pass_get_node_as_image (self,
                                                    render,
                                                    state,
                                                    gsk_opacity_node_get_child (node),
                                                    &tex_rect);
  if (image == NULL)
    return TRUE;

  gsk_vulkan_color_matrix_op_opacity (render,
                                      gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                                      image,
                                      &node->bounds,
                                      &state->offset,
                                      &tex_rect,
                                      gsk_opacity_node_get_opacity (node));

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_color_matrix_node (GskVulkanRenderPass       *self,
                                              GskVulkanRender           *render,
                                              const GskVulkanParseState *state,
                                              GskRenderNode             *node)
{
  GskVulkanImage *image;
  graphene_rect_t tex_rect;

  image = gsk_vulkan_render_pass_get_node_as_image (self,
                                                    render,
                                                    state,
                                                    gsk_color_matrix_node_get_child (node),
                                                    &tex_rect);
  if (image == NULL)
    return TRUE;

  gsk_vulkan_color_matrix_op (render,
                              gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                              image,
                              &node->bounds,
                              &state->offset,
                              &tex_rect,
                              gsk_color_matrix_node_get_color_matrix (node),
                              gsk_color_matrix_node_get_color_offset (node));

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_clip_node (GskVulkanRenderPass       *self,
                                      GskVulkanRender           *render,
                                      const GskVulkanParseState *state,
                                      GskRenderNode             *node)
{
  GskVulkanParseState new_state;
  graphene_rect_t clip;
  gboolean do_push_constants, do_scissor;

  graphene_rect_offset_r (gsk_clip_node_get_clip (node),
                          state->offset.x, state->offset.y,
                          &clip);

  /* Check if we can use scissoring for the clip */
  if (gsk_vulkan_parse_rect_is_integer (state, &clip, &new_state.scissor))
    {
      if (!gdk_rectangle_intersect (&new_state.scissor, &state->scissor, &new_state.scissor))
        return TRUE;

      if (gsk_vulkan_clip_intersect_rect (&new_state.clip, &state->clip, &clip))
        {
          if (new_state.clip.type == GSK_VULKAN_CLIP_RECT)
            new_state.clip.type = GSK_VULKAN_CLIP_NONE;

          do_push_constants = TRUE;
        }
      else
        {
          gsk_vulkan_clip_init_copy (&new_state.clip, &state->clip);
          do_push_constants = FALSE;
        }
      
      do_scissor = TRUE;
    }
  else
    {
      if (!gsk_vulkan_clip_intersect_rect (&new_state.clip, &state->clip, &clip))
        FALLBACK ("Failed to find intersection between clip of type %u and rectangle", state->clip.type);

      new_state.scissor = state->scissor;

      do_push_constants = TRUE;
      do_scissor = FALSE;
    }

  if (new_state.clip.type == GSK_VULKAN_CLIP_ALL_CLIPPED)
    return TRUE;

  new_state.offset = state->offset;
  graphene_vec2_init_from_vec2 (&new_state.scale, &state->scale);
  new_state.modelview = state->modelview;
  graphene_matrix_init_from_matrix (&new_state.projection, &state->projection);

  if (do_scissor)
    gsk_vulkan_render_pass_append_scissor (render, node, &new_state);
  if (do_push_constants)
    gsk_vulkan_render_pass_append_push_constants (render, node, &new_state);

  gsk_vulkan_render_pass_add_node (self, render, &new_state, gsk_clip_node_get_child (node));

  if (do_push_constants)
    gsk_vulkan_render_pass_append_push_constants (render, node, state);
  if (do_scissor)
    gsk_vulkan_render_pass_append_scissor (render, node, state);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_rounded_clip_node (GskVulkanRenderPass       *self,
                                              GskVulkanRender           *render,
                                              const GskVulkanParseState *state,
                                              GskRenderNode             *node)
{
  GskVulkanParseState new_state;
  GskRoundedRect clip;

  clip = *gsk_rounded_clip_node_get_clip (node);
  gsk_rounded_rect_offset (&clip, state->offset.x, state->offset.y);

  if (!gsk_vulkan_clip_intersect_rounded_rect (&new_state.clip, &state->clip, &clip))
    FALLBACK ("Failed to find intersection between clip of type %u and rounded rectangle", state->clip.type);

  if (new_state.clip.type == GSK_VULKAN_CLIP_ALL_CLIPPED)
    return TRUE;

  new_state.scissor = state->scissor;
  new_state.offset = state->offset;
  graphene_vec2_init_from_vec2 (&new_state.scale, &state->scale);
  new_state.modelview = state->modelview;
  graphene_matrix_init_from_matrix (&new_state.projection, &state->projection);

  gsk_vulkan_render_pass_append_push_constants (render, node, &new_state);

  gsk_vulkan_render_pass_add_node (self, render, &new_state, gsk_rounded_clip_node_get_child (node));

  gsk_vulkan_render_pass_append_push_constants (render, node, state);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_repeat_node (GskVulkanRenderPass       *self,
                                        GskVulkanRender           *render,
                                        const GskVulkanParseState *state,
                                        GskRenderNode             *node)
{
  const graphene_rect_t *child_bounds;
  GskVulkanImage *image;

  child_bounds = gsk_repeat_node_get_child_bounds (node);

  if (graphene_rect_get_area (child_bounds) == 0)
    return TRUE;

  image = gsk_vulkan_render_pass_op_offscreen (render,
                                               &state->scale,
                                               child_bounds,
                                               gsk_repeat_node_get_child (node));

  gsk_vulkan_texture_op (render,
                         gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                         image,
                         GSK_VULKAN_SAMPLER_REPEAT,
                         &node->bounds,
                         &state->offset,
                         child_bounds);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_blend_node (GskVulkanRenderPass       *self,
                                       GskVulkanRender           *render,
                                       const GskVulkanParseState *state,
                                       GskRenderNode             *node)
{
  GskRenderNode *top_child, *bottom_child;
  GskVulkanImage *top_image, *bottom_image;
  graphene_rect_t top_tex_rect, bottom_tex_rect;

  top_child = gsk_blend_node_get_top_child (node);
  top_image = gsk_vulkan_render_pass_get_node_as_image (self,
                                                        render,
                                                        state,
                                                        top_child,
                                                        &top_tex_rect);
  bottom_child = gsk_blend_node_get_bottom_child (node);
  bottom_image = gsk_vulkan_render_pass_get_node_as_image (self,
                                                           render,
                                                           state,
                                                           bottom_child,
                                                           &bottom_tex_rect);
  if (top_image == NULL)
    {
      if (bottom_image == NULL)
        return TRUE;

      top_image = bottom_image;
      top_tex_rect = *graphene_rect_zero ();
    }
  else if (bottom_image == NULL)
    {
      bottom_image = top_image;
      bottom_tex_rect = *graphene_rect_zero ();
    }

  gsk_vulkan_blend_mode_op (render,
                            gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                            &node->bounds,
                            &state->offset,
                            gsk_blend_node_get_blend_mode (node),
                            top_image,
                            &top_child->bounds,
                            &top_tex_rect,
                            bottom_image,
                            &bottom_child->bounds,
                            &bottom_tex_rect);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_cross_fade_node (GskVulkanRenderPass       *self,
                                            GskVulkanRender           *render,
                                            const GskVulkanParseState *state,
                                            GskRenderNode             *node)
{
  GskRenderNode *start_child, *end_child;
  GskVulkanImage *start_image, *end_image;
  graphene_rect_t start_tex_rect, end_tex_rect;
  float progress;

  progress = gsk_cross_fade_node_get_progress (node);
  start_child = gsk_cross_fade_node_get_start_child (node);
  start_image = gsk_vulkan_render_pass_get_node_as_image (self,
                                                          render,
                                                          state,
                                                          start_child,
                                                          &start_tex_rect);
  end_child = gsk_cross_fade_node_get_end_child (node);
  end_image = gsk_vulkan_render_pass_get_node_as_image (self,
                                                        render,
                                                        state,
                                                        end_child,
                                                        &end_tex_rect);
  if (start_image == NULL)
    {
      if (end_image == NULL)
        return TRUE;

      gsk_vulkan_color_matrix_op_opacity (render,
                                          gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &end_child->bounds),
                                          end_image,
                                          &node->bounds,
                                          &state->offset,
                                          &end_tex_rect,
                                          progress);

      return TRUE;
    }
  else if (end_image == NULL)
    {
      gsk_vulkan_color_matrix_op_opacity (render,
                                          gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &start_child->bounds),
                                          start_image,
                                          &node->bounds,
                                          &state->offset,
                                          &start_tex_rect,
                                          1.0f - progress);
      return TRUE;
    }

  gsk_vulkan_cross_fade_op (render,
                            gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                            &node->bounds,
                            &state->offset,
                            progress,
                            start_image,
                            &start_child->bounds,
                            &start_tex_rect,
                            end_image,
                            &end_child->bounds,
                            &end_tex_rect);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_text_node (GskVulkanRenderPass       *self,
                                      GskVulkanRender           *render,
                                      const GskVulkanParseState *state,
                                      GskRenderNode             *node)
{
  const PangoGlyphInfo *glyphs;
  GskVulkanGlyphCache *cache;
  const graphene_point_t *node_offset;
  const PangoFont *font;
  guint num_glyphs;
  int x_position;
  int i;
  float scale;

  cache = gsk_vulkan_renderer_get_glyph_cache (GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render)));
  num_glyphs = gsk_text_node_get_num_glyphs (node);
  glyphs = gsk_text_node_get_glyphs (node, NULL);
  font = gsk_text_node_get_font (node);

  scale = MAX (graphene_vec2_get_x (&state->scale), graphene_vec2_get_y (&state->scale));
  node_offset = gsk_text_node_get_offset (node);

  x_position = 0;
  for (i = 0; i < num_glyphs; i++)
    {
      GskVulkanCachedGlyph *glyph;
      const PangoGlyphInfo *gi = &glyphs[i];
      graphene_rect_t glyph_bounds, glyph_tex_rect;

      glyph = gsk_vulkan_glyph_cache_lookup (cache,
                                             render,
                                             (PangoFont *)font,
                                             gi->glyph,
                                             x_position + gi->geometry.x_offset,
                                             gi->geometry.y_offset,
                                             scale);

      glyph_bounds = GRAPHENE_RECT_INIT (glyph->draw_x + node_offset->x + (float) (x_position + gi->geometry.x_offset) / PANGO_SCALE,
                                         glyph->draw_y + node_offset->y + (float) (gi->geometry.y_offset) / PANGO_SCALE,
                                         glyph->draw_width,
                                         glyph->draw_height);
      graphene_rect_init (&glyph_tex_rect,
                          glyph_bounds.origin.x - glyph->draw_width * glyph->tx / glyph->tw,
                          glyph_bounds.origin.y - glyph->draw_height * glyph->ty / glyph->th,
                          glyph->draw_width / glyph->tw,
                          glyph->draw_height / glyph->th);
      if (gsk_text_node_has_color_glyphs (node))
        gsk_vulkan_texture_op (render,
                               gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &glyph_bounds),
                               glyph->atlas_image,
                               GSK_VULKAN_SAMPLER_DEFAULT,
                               &glyph_bounds,
                               &state->offset,
                               &glyph_tex_rect);
      else
        gsk_vulkan_glyph_op (render,
                             gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &glyph_bounds),
                             glyph->atlas_image,
                             &glyph_bounds,
                             &state->offset,
                             &glyph_tex_rect,
                             gsk_text_node_get_color (node));

      x_position += gi->geometry.width;
    }

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_blur_node (GskVulkanRenderPass       *self,
                                      GskVulkanRender           *render,
                                      const GskVulkanParseState *state,
                                      GskRenderNode             *node)
{
  GskVulkanImage *image;
  graphene_rect_t tex_rect;
  float radius;

  radius = gsk_blur_node_get_radius (node);
  if (radius == 0)
    {
      gsk_vulkan_render_pass_add_node (self, render, state, gsk_blur_node_get_child (node));
      return TRUE;
    }

  image = gsk_vulkan_render_pass_get_node_as_image (self,
                                                    render,
                                                    state,
                                                    gsk_blur_node_get_child (node),
                                                    &tex_rect);
  if (image == NULL)
    return TRUE;

  gsk_vulkan_blur_op (render,
                      gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                      image,
                      &node->bounds,
                      &state->offset,
                      &tex_rect,
                      radius);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_mask_node (GskVulkanRenderPass       *self,
                                      GskVulkanRender           *render,
                                      const GskVulkanParseState *state,
                                      GskRenderNode             *node)
{
  GskVulkanImage *source_image, *mask_image;
  graphene_rect_t source_tex_rect, mask_tex_rect;
  GskRenderNode *source, *mask;
  GskMaskMode mode;

  mode = gsk_mask_node_get_mask_mode (node);
  source = gsk_mask_node_get_source (node);
  mask = gsk_mask_node_get_mask (node);
  mask_image = gsk_vulkan_render_pass_get_node_as_image (self,
                                                         render,
                                                         state,
                                                         mask,
                                                         &mask_tex_rect);
  if (mask_image == NULL)
    {
      if (mode == GSK_MASK_MODE_INVERTED_ALPHA)
        gsk_vulkan_render_pass_add_node (self, render, state, source);

      return TRUE;
    }

  /* Use the glyph shader as an optimization */
  if (mode == GSK_MASK_MODE_ALPHA &&
      gsk_render_node_get_node_type (source) == GSK_COLOR_NODE)
    {
      graphene_rect_t bounds;
      if (graphene_rect_intersection (&source->bounds, &mask->bounds, &bounds))
        gsk_vulkan_glyph_op (render,
                             gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &bounds),
                             mask_image,
                             &bounds,
                             &state->offset,
                             &mask_tex_rect,
                             gsk_color_node_get_color (source));
      return TRUE;
    }

  source_image = gsk_vulkan_render_pass_get_node_as_image (self,
                                                           render,
                                                           state,
                                                           source,
                                                           &source_tex_rect);
  if (source_image == NULL)
    return TRUE;

  gsk_vulkan_mask_op (render,
                      gsk_vulkan_clip_get_shader_clip (&state->clip, &state->offset, &node->bounds),
                      &state->offset,
                      source_image,
                      &source->bounds,
                      &source_tex_rect,
                      mask_image,
                      &mask->bounds,
                      &mask_tex_rect,
                      mode);
  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_debug_node (GskVulkanRenderPass       *self,
                                       GskVulkanRender           *render,
                                       const GskVulkanParseState *state,
                                       GskRenderNode             *node)
{
  gsk_vulkan_render_pass_add_node (self, render, state, gsk_debug_node_get_child (node));
  return TRUE;
}

#undef FALLBACK

typedef gboolean (*GskVulkanRenderPassNodeFunc) (GskVulkanRenderPass       *self,
                                                 GskVulkanRender           *render,
                                                 const GskVulkanParseState *state,
                                                 GskRenderNode             *node);

/* TODO: implement remaining nodes */
static const GskVulkanRenderPassNodeFunc nodes_vtable[] = {
  [GSK_NOT_A_RENDER_NODE] = gsk_vulkan_render_pass_implode,
  [GSK_CONTAINER_NODE] = gsk_vulkan_render_pass_add_container_node,
  [GSK_CAIRO_NODE] = gsk_vulkan_render_pass_add_cairo_node,
  [GSK_COLOR_NODE] = gsk_vulkan_render_pass_add_color_node,
  [GSK_LINEAR_GRADIENT_NODE] = gsk_vulkan_render_pass_add_linear_gradient_node,
  [GSK_REPEATING_LINEAR_GRADIENT_NODE] = gsk_vulkan_render_pass_add_linear_gradient_node,
  [GSK_RADIAL_GRADIENT_NODE] = NULL,
  [GSK_REPEATING_RADIAL_GRADIENT_NODE] = NULL,
  [GSK_CONIC_GRADIENT_NODE] = NULL,
  [GSK_BORDER_NODE] = gsk_vulkan_render_pass_add_border_node,
  [GSK_TEXTURE_NODE] = gsk_vulkan_render_pass_add_texture_node,
  [GSK_INSET_SHADOW_NODE] = gsk_vulkan_render_pass_add_inset_shadow_node,
  [GSK_OUTSET_SHADOW_NODE] = gsk_vulkan_render_pass_add_outset_shadow_node,
  [GSK_TRANSFORM_NODE] = gsk_vulkan_render_pass_add_transform_node,
  [GSK_OPACITY_NODE] = gsk_vulkan_render_pass_add_opacity_node,
  [GSK_COLOR_MATRIX_NODE] = gsk_vulkan_render_pass_add_color_matrix_node,
  [GSK_REPEAT_NODE] = gsk_vulkan_render_pass_add_repeat_node,
  [GSK_CLIP_NODE] = gsk_vulkan_render_pass_add_clip_node,
  [GSK_ROUNDED_CLIP_NODE] = gsk_vulkan_render_pass_add_rounded_clip_node,
  [GSK_SHADOW_NODE] = NULL,
  [GSK_BLEND_NODE] = gsk_vulkan_render_pass_add_blend_node,
  [GSK_CROSS_FADE_NODE] = gsk_vulkan_render_pass_add_cross_fade_node,
  [GSK_TEXT_NODE] = gsk_vulkan_render_pass_add_text_node,
  [GSK_BLUR_NODE] = gsk_vulkan_render_pass_add_blur_node,
  [GSK_DEBUG_NODE] = gsk_vulkan_render_pass_add_debug_node,
  [GSK_GL_SHADER_NODE] = NULL,
  [GSK_TEXTURE_SCALE_NODE] = gsk_vulkan_render_pass_add_texture_scale_node,
  [GSK_MASK_NODE] = gsk_vulkan_render_pass_add_mask_node,
};

static void
gsk_vulkan_render_pass_add_node (GskVulkanRenderPass       *self,
                                 GskVulkanRender           *render,
                                 const GskVulkanParseState *state,
                                 GskRenderNode             *node)
{
  GskVulkanRenderPassNodeFunc node_func;
  GskRenderNodeType node_type;
  gboolean fallback = FALSE;

  /* This catches the corner cases of empty nodes, so after this check
   * there's quaranteed to be at least 1 pixel that needs to be drawn */
  if (!gsk_vulkan_clip_may_intersect_rect (&state->clip, &state->offset, &node->bounds))
    return;

  node_type = gsk_render_node_get_node_type (node);
  if (node_type < G_N_ELEMENTS (nodes_vtable))
    node_func = nodes_vtable[node_type];
  else
    node_func = NULL;

  if (node_func)
    {
      if (!node_func (self, render, state, node))
        fallback = TRUE;
    }
  else
    {
      GSK_RENDERER_DEBUG (gsk_vulkan_render_get_renderer (render),
                          FALLBACK, "Unsupported node '%s'",
                          g_type_name_from_instance ((GTypeInstance *) node));
      fallback = TRUE;
    }

  if (fallback)
    gsk_vulkan_render_pass_add_fallback_node (self, render, state, node);
}

void
gsk_vulkan_render_pass_add (GskVulkanRenderPass   *self,
                            GskVulkanRender       *render,
                            int                    width,
                            int                    height,
                            cairo_rectangle_int_t *clip,
                            GskRenderNode         *node,
                            const graphene_rect_t *viewport)
{
  GskVulkanParseState state;

  state.scissor = *clip;
  gsk_vulkan_clip_init_empty (&state.clip, &GRAPHENE_RECT_INIT (0, 0, viewport->size.width, viewport->size.height));

  state.modelview = NULL;
  graphene_matrix_init_ortho (&state.projection,
                              0, width,
                              0, height,
                              2 * ORTHO_NEAR_PLANE - ORTHO_FAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_vec2_init (&state.scale, width / viewport->size.width,
                                    height / viewport->size.height);
  state.offset = GRAPHENE_POINT_INIT (-viewport->origin.x,
                                      -viewport->origin.y);

  gsk_vulkan_render_pass_append_scissor (render, node, &state);
  gsk_vulkan_render_pass_append_push_constants (render, node, &state);

  gsk_vulkan_render_pass_add_node (self, render, &state, node);
}

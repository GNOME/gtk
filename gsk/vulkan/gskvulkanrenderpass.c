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
#include "gskvulkanclipprivate.h"
#include "gskvulkancolormatrixopprivate.h"
#include "gskvulkancoloropprivate.h"
#include "gskvulkancrossfadeopprivate.h"
#include "gskvulkanglyphopprivate.h"
#include "gskvulkaninsetshadowopprivate.h"
#include "gskvulkanlineargradientopprivate.h"
#include "gskvulkanmaskopprivate.h"
#include "gskvulkanopprivate.h"
#include "gskvulkanprivate.h"
#include "gskvulkanrendererprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanoffscreenopprivate.h"
#include "gskvulkanoutsetshadowopprivate.h"
#include "gskvulkanpushconstantsopprivate.h"
#include "gskvulkanscissoropprivate.h"
#include "gskvulkantextureopprivate.h"
#include "gskvulkanuploadcairoopprivate.h"
#include "gskvulkanuploadopprivate.h"
#include "gskprivate.h"

#include "gdk/gdkvulkancontextprivate.h"

#define GDK_ARRAY_NAME gsk_vulkan_render_ops
#define GDK_ARRAY_TYPE_NAME GskVulkanRenderOps
#define GDK_ARRAY_ELEMENT_TYPE guchar
#define GDK_ARRAY_BY_VALUE 1
#include "gdk/gdkarrayimpl.c"

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

typedef struct _GskVulkanParseState GskVulkanParseState;

struct _GskVulkanRenderPass
{
  GdkVulkanContext *vulkan;

  GskVulkanRenderOps render_ops;

  GskVulkanImage *target;
  graphene_rect_t viewport;
  cairo_region_t *clip;

  graphene_vec2_t scale;

  VkRenderPass render_pass;
  VkFramebuffer framebuffer;
  VkSemaphore signal_semaphore;
  GArray *wait_semaphores;
  GskVulkanBuffer *vertex_data;
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

#ifdef G_ENABLE_DEBUG
static GQuark fallback_pixels_quark;
static GQuark texture_pixels_quark;
#endif

static void
gsk_vulkan_render_pass_add (GskVulkanRenderPass *self,
                            GskVulkanRender     *render,
                            GskRenderNode       *node);

GskVulkanRenderPass *
gsk_vulkan_render_pass_new (GdkVulkanContext      *context,
                            GskVulkanRender       *render,
                            GskVulkanImage        *target,
                            const graphene_vec2_t *scale,
                            const graphene_rect_t *viewport,
                            cairo_region_t        *clip,
                            GskRenderNode         *node,
                            VkSemaphore            signal_semaphore)
{
  GskVulkanRenderPass *self;
  VkImageLayout final_layout;

  self = g_new0 (GskVulkanRenderPass, 1);
  self->vulkan = g_object_ref (context);
  gsk_vulkan_render_ops_init (&self->render_ops);

  self->target = g_object_ref (target);
  self->clip = cairo_region_copy (clip);
  self->viewport = *viewport;
  graphene_vec2_init_from_vec2 (&self->scale, scale);

  if (signal_semaphore != VK_NULL_HANDLE) // this is a dependent pass
    final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  else
    final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  GSK_VK_CHECK (vkCreateRenderPass, gdk_vulkan_context_get_device (self->vulkan),
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = gsk_vulkan_image_get_vk_format (target),
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                              .finalLayout = final_layout
                                           }
                                        },
                                        .subpassCount = 1,
                                        .pSubpasses = (VkSubpassDescription []) {
                                           {
                                              .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                              .inputAttachmentCount = 0,
                                              .colorAttachmentCount = 1,
                                              .pColorAttachments = (VkAttachmentReference []) {
                                                 {
                                                    .attachment = 0,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pResolveAttachments = (VkAttachmentReference []) {
                                                  {
                                                     .attachment = VK_ATTACHMENT_UNUSED,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pDepthStencilAttachment = NULL,
                                            }
                                         },
                                         .dependencyCount = 0
                                      },
                                      NULL,
                                      &self->render_pass);

  GSK_VK_CHECK (vkCreateFramebuffer, gdk_vulkan_context_get_device (self->vulkan),
                                     &(VkFramebufferCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                         .renderPass = self->render_pass,
                                         .attachmentCount = 1,
                                         .pAttachments = (VkImageView[1]) {
                                             gsk_vulkan_image_get_image_view (target)
                                         },
                                         .width = gsk_vulkan_image_get_width (target),
                                         .height = gsk_vulkan_image_get_height (target),
                                         .layers = 1
                                     },
                                     NULL,
                                     &self->framebuffer);

  self->signal_semaphore = signal_semaphore;
  self->wait_semaphores = g_array_new (FALSE, FALSE, sizeof (VkSemaphore));
  self->vertex_data = NULL;

#ifdef G_ENABLE_DEBUG
  if (fallback_pixels_quark == 0)
    {
      fallback_pixels_quark = g_quark_from_static_string ("fallback-pixels");
      texture_pixels_quark = g_quark_from_static_string ("texture-pixels");
    }
#endif

  gsk_vulkan_render_pass_add (self, render, node);

  return self;
}

void
gsk_vulkan_render_pass_free (GskVulkanRenderPass *self)
{
  VkDevice device = gdk_vulkan_context_get_device (self->vulkan);
  GskVulkanOp *op;
  gsize i;

  for (i = 0; i < gsk_vulkan_render_ops_get_size (&self->render_ops); i += op->op_class->size)
    {
      op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, i);

      gsk_vulkan_op_finish (op);
    }
  gsk_vulkan_render_ops_clear (&self->render_ops);

  g_object_unref (self->vulkan);
  g_object_unref (self->target);
  cairo_region_destroy (self->clip);
  vkDestroyFramebuffer (device, self->framebuffer, NULL);
  vkDestroyRenderPass (device, self->render_pass, NULL);

  if (self->vertex_data)
    gsk_vulkan_buffer_free (self->vertex_data);
  if (self->signal_semaphore != VK_NULL_HANDLE)
    vkDestroySemaphore (device, self->signal_semaphore, NULL);
  g_array_unref (self->wait_semaphores);

  g_free (self);
}

void
gsk_vulkan_render_pass_print (GskVulkanRenderPass *self,
                              GString             *string,
                              guint                indent)
{
  GskVulkanOp *op;
  gsize i;

  for (i = 0; i < gsk_vulkan_render_ops_get_size (&self->render_ops); i += op->op_class->size)
    {
      op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, i);

      gsk_vulkan_op_print (op, string, indent);
    }
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

gpointer
gsk_vulkan_render_pass_alloc_op (GskVulkanRenderPass *self,
                                 gsize                size)
{
  gsize pos;

  pos = gsk_vulkan_render_ops_get_size (&self->render_ops);

  gsk_vulkan_render_ops_splice (&self->render_ops,
                                pos,
                                0, FALSE,
                                NULL,
                                size);

  return gsk_vulkan_render_ops_index (&self->render_ops, pos);
}

static void
gsk_vulkan_render_pass_append_scissor (GskVulkanRenderPass       *self,
                                       GskRenderNode             *node,
                                       const GskVulkanParseState *state)
{
  gsk_vulkan_scissor_op (self, &state->scissor);
}

static void
gsk_vulkan_render_pass_append_push_constants (GskVulkanRenderPass       *self,
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

  gsk_vulkan_push_constants_op (self, &state->scale, &mvp, &state->clip.rect);
}

#define FALLBACK(...) G_STMT_START { \
  GSK_RENDERER_DEBUG (gsk_vulkan_render_get_renderer (render), FALLBACK, __VA_ARGS__); \
  return FALSE; \
}G_STMT_END

static GskVulkanImage *
gsk_vulkan_render_pass_get_node_as_image (GskVulkanRenderPass       *self,
                                          GskVulkanRender           *render,
                                          const GskVulkanParseState *state,
                                          GskRenderNode             *node,
                                          graphene_rect_t           *tex_bounds)
{
  VkSemaphore semaphore;
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
            result = gsk_vulkan_upload_op (self, self->vulkan, texture);
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

        result = gsk_vulkan_upload_cairo_op (self,
                                             self->vulkan,
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

        vkCreateSemaphore (gdk_vulkan_context_get_device (self->vulkan),
                           &(VkSemaphoreCreateInfo) {
                             VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                             NULL,
                             0
                           },
                           NULL,
                           &semaphore);
        g_array_append_val (self->wait_semaphores, semaphore);

        result = gsk_vulkan_offscreen_op (self,
                                          self->vulkan,
                                          render,
                                          &state->scale,
                                          &clipped,
                                          semaphore,
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

  image = gsk_vulkan_upload_cairo_op (self,
                                      self->vulkan,
                                      node,
                                      &state->scale,
                                      &clipped);

  gsk_vulkan_texture_op (self,
                         gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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
  gsk_vulkan_color_op (self,
                       gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
                       &node->bounds,
                       &state->offset,
                       gsk_color_node_get_color (node));

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_linear_gradient_node (GskVulkanRenderPass       *self,
                                                 GskVulkanRender           *render,
                                                 const GskVulkanParseState *state,
                                                 GskRenderNode             *node)
{
  gsk_vulkan_linear_gradient_op (self,
                                 gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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
  gsk_vulkan_border_op (self,
                        gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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
      image = gsk_vulkan_upload_op (self, self->vulkan, texture);
      gsk_vulkan_renderer_add_texture_image (renderer, texture, image);
    }

  gsk_vulkan_texture_op (self,
                         gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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
      image = gsk_vulkan_upload_op (self, self->vulkan, texture);
      gsk_vulkan_renderer_add_texture_image (renderer, texture, image);
    }

  gsk_vulkan_texture_op (self,
                         gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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

  gsk_vulkan_inset_shadow_op (self,
                              gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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

  gsk_vulkan_outset_shadow_op (self,
                               gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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

  gsk_vulkan_render_pass_append_push_constants (self, node, &new_state);

  gsk_vulkan_render_pass_add_node (self, render, &new_state, child);

  gsk_vulkan_render_pass_append_push_constants (self, node, state);

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

  gsk_vulkan_color_matrix_op_opacity (self,
                                      gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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

  gsk_vulkan_color_matrix_op (self,
                              gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
                              image,
                              &node->bounds,
                              &state->offset,
                              &tex_rect,
                              gsk_color_matrix_node_get_color_matrix (node),
                              gsk_color_matrix_node_get_color_offset (node));

  return TRUE;
}

static gboolean
clip_can_be_scissored (const graphene_rect_t *rect,
                       const graphene_vec2_t *scale,
                       GskTransform          *modelview,
                       cairo_rectangle_int_t *int_rect)
{
  graphene_rect_t transformed_rect;
  float scale_x = graphene_vec2_get_x (scale);
  float scale_y = graphene_vec2_get_y (scale);

  switch (gsk_transform_get_category (modelview))
    {
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
      return FALSE;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      gsk_transform_transform_bounds (modelview, rect, &transformed_rect);
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
  if (clip_can_be_scissored (&clip, &state->scale, state->modelview, &new_state.scissor))
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
    gsk_vulkan_render_pass_append_scissor (self, node, &new_state);
  if (do_push_constants)
    gsk_vulkan_render_pass_append_push_constants (self, node, &new_state);

  gsk_vulkan_render_pass_add_node (self, render, &new_state, gsk_clip_node_get_child (node));

  if (do_push_constants)
    gsk_vulkan_render_pass_append_push_constants (self, node, state);
  if (do_scissor)
    gsk_vulkan_render_pass_append_scissor (self, node, state);

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

  gsk_vulkan_render_pass_append_push_constants (self, node, &new_state);

  gsk_vulkan_render_pass_add_node (self, render, &new_state, gsk_rounded_clip_node_get_child (node));

  gsk_vulkan_render_pass_append_push_constants (self, node, state);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_repeat_node (GskVulkanRenderPass       *self,
                                        GskVulkanRender           *render,
                                        const GskVulkanParseState *state,
                                        GskRenderNode             *node)
{
  const graphene_rect_t *child_bounds;
  VkSemaphore semaphore;
  GskVulkanImage *image;

  child_bounds = gsk_repeat_node_get_child_bounds (node);

  if (graphene_rect_get_area (child_bounds) == 0)
    return TRUE;

  /* We need to create a texture in the right size so that we can repeat it
   * properly, so even for texture nodes this step is necessary.
   * We also can't use the clip because of that. */
  vkCreateSemaphore (gdk_vulkan_context_get_device (self->vulkan),
                     &(VkSemaphoreCreateInfo) {
                       VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                       NULL,
                       0
                     },
                     NULL,
                     &semaphore);

  g_array_append_val (self->wait_semaphores, semaphore);

  image = gsk_vulkan_offscreen_op (self,
                                   self->vulkan,
                                   render,
                                   &state->scale,
                                   child_bounds,
                                   semaphore,
                                   gsk_repeat_node_get_child (node));

  gsk_vulkan_texture_op (self,
                         gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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

  gsk_vulkan_blend_mode_op (self,
                            gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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

      gsk_vulkan_color_matrix_op_opacity (self,
                                          gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &end_child->bounds),
                                          end_image,
                                          &node->bounds,
                                          &state->offset,
                                          &end_tex_rect,
                                          progress);

      return TRUE;
    }
  else if (end_image == NULL)
    {
      gsk_vulkan_color_matrix_op_opacity (self,
                                          gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &start_child->bounds),
                                          start_image,
                                          &node->bounds,
                                          &state->offset,
                                          &start_tex_rect,
                                          1.0f - progress);
      return TRUE;
    }

  gsk_vulkan_cross_fade_op (self,
                            gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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
  GskVulkanRenderer *renderer;
  const graphene_point_t *node_offset;
  const PangoFont *font;
  guint num_glyphs;
  int x_position;
  int i;
  float scale;

  renderer = GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render));
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

      glyph = gsk_vulkan_renderer_cache_glyph (renderer,
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
        gsk_vulkan_texture_op (self,
                               gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &glyph_bounds),
                               glyph->atlas_image,
                               GSK_VULKAN_SAMPLER_DEFAULT,
                               &glyph_bounds,
                               &state->offset,
                               &glyph_tex_rect);
      else
        gsk_vulkan_glyph_op (self,
                             gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &glyph_bounds),
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

  gsk_vulkan_blur_op (self,
                      gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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
        gsk_vulkan_glyph_op (self,
                             gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &bounds),
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

  gsk_vulkan_mask_op (self,
                      gsk_vulkan_clip_get_clip_type (&state->clip, &state->offset, &node->bounds),
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

static void
gsk_vulkan_render_pass_add (GskVulkanRenderPass *self,
                            GskVulkanRender     *render,
                            GskRenderNode       *node)
{
  GskVulkanParseState state;
  graphene_rect_t clip;
  float scale_x, scale_y;

  scale_x = 1 / graphene_vec2_get_x (&self->scale);
  scale_y = 1 / graphene_vec2_get_y (&self->scale);
  cairo_region_get_extents (self->clip, &state.scissor);
  clip = GRAPHENE_RECT_INIT(state.scissor.x, state.scissor.y,
                            state.scissor.width, state.scissor.height);
  graphene_rect_scale (&clip, scale_x, scale_y, &clip);
  gsk_vulkan_clip_init_empty (&state.clip, &clip);

  state.modelview = NULL;
  graphene_matrix_init_ortho (&state.projection,
                              0, self->viewport.size.width,
                              0, self->viewport.size.height,
                              2 * ORTHO_NEAR_PLANE - ORTHO_FAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_vec2_init_from_vec2 (&state.scale, &self->scale);
  state.offset = GRAPHENE_POINT_INIT (-self->viewport.origin.x * scale_x,
                                      -self->viewport.origin.y * scale_y);

  gsk_vulkan_render_pass_append_scissor (self, node, &state);
  gsk_vulkan_render_pass_append_push_constants (self, node, &state);

  gsk_vulkan_render_pass_add_node (self, render, &state, node);
}

void
gsk_vulkan_render_pass_upload (GskVulkanRenderPass  *self,
                               GskVulkanRender      *render,
                               GskVulkanUploader    *uploader)
{
  GskVulkanOp *op;
  guint i;

  for (i = 0; i < gsk_vulkan_render_ops_get_size (&self->render_ops); i += op->op_class->size)
    {
      op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, i);

      gsk_vulkan_op_upload (op, self, render, uploader);
    }
}

static gsize
gsk_vulkan_render_pass_count_vertex_data (GskVulkanRenderPass *self)
{
  GskVulkanOp *op;
  gsize n_bytes;
  guint i;

  n_bytes = 0;
  for (i = 0; i < gsk_vulkan_render_ops_get_size (&self->render_ops); i += op->op_class->size)
    {
      op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, i);

      n_bytes = gsk_vulkan_op_count_vertex_data (op, n_bytes);
    }

  return n_bytes;
}

static void
gsk_vulkan_render_pass_collect_vertex_data (GskVulkanRenderPass *self,
                                            GskVulkanRender     *render,
                                            guchar              *data)
{
  GskVulkanOp *op;
  guint i;

  for (i = 0; i < gsk_vulkan_render_ops_get_size (&self->render_ops); i += op->op_class->size)
    {
      op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, i);

      gsk_vulkan_op_collect_vertex_data (op, self, render, data);
    }
}

static GskVulkanBuffer *
gsk_vulkan_render_pass_get_vertex_data (GskVulkanRenderPass *self,
                                        GskVulkanRender     *render)
{
  if (self->vertex_data == NULL)
    {
      gsize n_bytes;
      guchar *data;

      n_bytes = gsk_vulkan_render_pass_count_vertex_data (self);
      if (n_bytes == 0)
        return NULL;

      self->vertex_data = gsk_vulkan_buffer_new (self->vulkan, n_bytes);
      data = gsk_vulkan_buffer_map (self->vertex_data);
      gsk_vulkan_render_pass_collect_vertex_data (self, render, data);
      gsk_vulkan_buffer_unmap (self->vertex_data);
    }

  return self->vertex_data;
}

gsize
gsk_vulkan_render_pass_get_wait_semaphores (GskVulkanRenderPass  *self,
                                            VkSemaphore         **semaphores)
{
  *semaphores = (VkSemaphore *)self->wait_semaphores->data;
  return self->wait_semaphores->len;
}

gsize
gsk_vulkan_render_pass_get_signal_semaphores (GskVulkanRenderPass  *self,
                                              VkSemaphore         **semaphores)
{
  *semaphores = (VkSemaphore *)&self->signal_semaphore;
  return self->signal_semaphore != VK_NULL_HANDLE ? 1 : 0;
}

void
gsk_vulkan_render_pass_reserve_descriptor_sets (GskVulkanRenderPass *self,
                                                GskVulkanRender     *render)
{
  GskVulkanOp *op;
  guint i;

  for (i = 0; i < gsk_vulkan_render_ops_get_size (&self->render_ops); i += op->op_class->size)
    {
      op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, i);

      gsk_vulkan_op_reserve_descriptor_sets (op, render);
    }
}

static void
gsk_vulkan_render_pass_draw_rect (GskVulkanRenderPass     *self,
                                  GskVulkanRender         *render,
                                  VkPipelineLayout         pipeline_layout,
                                  VkCommandBuffer          command_buffer)
{
  VkPipeline current_pipeline = VK_NULL_HANDLE;
  VkPipeline op_pipeline;
  GskVulkanOp *op;
  guint i;
  GskVulkanBuffer *vertex_buffer;

  vertex_buffer = gsk_vulkan_render_pass_get_vertex_data (self, render);

  if (vertex_buffer)
    vkCmdBindVertexBuffers (command_buffer,
                            0,
                            1,
                            (VkBuffer[1]) {
                                gsk_vulkan_buffer_get_buffer (vertex_buffer)
                            },
                            (VkDeviceSize[1]) { 0 });

  for (i = 0; i < gsk_vulkan_render_ops_get_size (&self->render_ops); i += op->op_class->size)
    {
      op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, i);

      if (op->op_class->shader_name)
        {
          op_pipeline = gsk_vulkan_render_create_pipeline (render,
                                                           op->op_class->shader_name,
                                                           op->clip_type,
                                                           op->op_class->vertex_input_state,
                                                           gsk_vulkan_image_get_vk_format (self->target),
                                                           self->render_pass);

          if (op_pipeline != current_pipeline)
            {
              current_pipeline = op_pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 current_pipeline);
            }
        }

      gsk_vulkan_op_command (op, render, pipeline_layout, command_buffer);
    }
}

void
gsk_vulkan_render_pass_draw (GskVulkanRenderPass *self,
                             GskVulkanRender     *render,
                             VkPipelineLayout     pipeline_layout,
                             VkCommandBuffer      command_buffer)
{
  cairo_rectangle_int_t rect;

  vkCmdSetViewport (command_buffer,
                    0,
                    1,
                    &(VkViewport) {
                        .x = 0,
                        .y = 0,
                        .width = self->viewport.size.width,
                        .height = self->viewport.size.height,
                        .minDepth = 0,
                        .maxDepth = 1
                    });

  cairo_region_get_extents (self->clip, &rect);

  vkCmdBeginRenderPass (command_buffer,
                        &(VkRenderPassBeginInfo) {
                            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                            .renderPass = self->render_pass,
                            .framebuffer = self->framebuffer,
                            .renderArea = { 
                                { rect.x, rect.y },
                                { rect.width, rect.height }
                            },
                            .clearValueCount = 1,
                            .pClearValues = (VkClearValue [1]) {
                                { .color = { .float32 = { 0.f, 0.f, 0.f, 0.f } } }
                            }
                        },
                        VK_SUBPASS_CONTENTS_INLINE);

  gsk_vulkan_render_bind_descriptor_sets (render, command_buffer);

  gsk_vulkan_render_pass_draw_rect (self, render, pipeline_layout, command_buffer);

  vkCmdEndRenderPass (command_buffer);
}


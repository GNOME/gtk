#include "config.h"

#include "gskvulkanrenderpassprivate.h"

#include "gskdebugprivate.h"
#include "gskprofilerprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderer.h"
#include "gskrendererprivate.h"
#include "gskroundedrectprivate.h"
#include "gsktransform.h"
#include "gskvulkanblendmodepipelineprivate.h"
#include "gskvulkanblurpipelineprivate.h"
#include "gskvulkanborderpipelineprivate.h"
#include "gskvulkanboxshadowpipelineprivate.h"
#include "gskvulkanclipprivate.h"
#include "gskvulkancolorpipelineprivate.h"
#include "gskvulkancolortextpipelineprivate.h"
#include "gskvulkancrossfadepipelineprivate.h"
#include "gskvulkaneffectpipelineprivate.h"
#include "gskvulkanlineargradientpipelineprivate.h"
#include "gskvulkantextpipelineprivate.h"
#include "gskvulkantexturepipelineprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanpushconstantsprivate.h"
#include "gskvulkanrendererprivate.h"
#include "gskprivate.h"

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

typedef union _GskVulkanOp GskVulkanOp;
typedef struct _GskVulkanOpRender GskVulkanOpRender;
typedef struct _GskVulkanOpText GskVulkanOpText;
typedef struct _GskVulkanOpPushConstants GskVulkanOpPushConstants;

typedef enum {
  /* GskVulkanOpRender */
  GSK_VULKAN_OP_FALLBACK,
  GSK_VULKAN_OP_FALLBACK_CLIP,
  GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP,
  GSK_VULKAN_OP_TEXTURE,
  GSK_VULKAN_OP_COLOR,
  GSK_VULKAN_OP_LINEAR_GRADIENT,
  GSK_VULKAN_OP_OPACITY,
  GSK_VULKAN_OP_BLUR,
  GSK_VULKAN_OP_COLOR_MATRIX,
  GSK_VULKAN_OP_BORDER,
  GSK_VULKAN_OP_INSET_SHADOW,
  GSK_VULKAN_OP_OUTSET_SHADOW,
  GSK_VULKAN_OP_REPEAT,
  GSK_VULKAN_OP_CROSS_FADE,
  GSK_VULKAN_OP_BLEND_MODE,
  /* GskVulkanOpText */
  GSK_VULKAN_OP_TEXT,
  GSK_VULKAN_OP_COLOR_TEXT,
  /* GskVulkanOpPushConstants */
  GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS,
} GskVulkanOpType;

/* render ops with 0, 1 or 2 sources */
struct _GskVulkanOpRender
{
  GskVulkanOpType      type;
  GskRenderNode       *node; /* node that's the source of this op */
  graphene_point_t     offset; /* offset of the node */
  GskVulkanPipeline   *pipeline; /* pipeline to use */
  GskRoundedRect       clip; /* clip rect (or random memory if not relevant) */
  GskVulkanImage      *source; /* source image to render */
  GskVulkanImage      *source2; /* second source image to render (if relevant) */
  gsize                vertex_offset; /* offset into vertex buffer */
  gsize                vertex_count; /* number of vertices */
  gsize                descriptor_set_index; /* index into descriptor sets array for the right descriptor set to bind */
  gsize                descriptor_set_index2; /* descriptor index for the second source (if relevant) */
  graphene_rect_t      source_rect; /* area that source maps to */
  graphene_rect_t      source2_rect; /* area that source2 maps to */
};

struct _GskVulkanOpText
{
  GskVulkanOpType      type;
  GskRenderNode       *node; /* node that's the source of this op */
  graphene_point_t     offset; /* offset of the node */
  GskVulkanPipeline   *pipeline; /* pipeline to use */
  GskRoundedRect       clip; /* clip rect (or random memory if not relevant) */
  GskVulkanImage      *source; /* source image to render */
  gsize                vertex_offset; /* offset into vertex buffer */
  gsize                vertex_count; /* number of vertices */
  gsize                descriptor_set_index; /* index into descriptor sets array for the right descriptor set to bind */
  guint                texture_index; /* index of the texture in the glyph cache */
  guint                start_glyph; /* the first glyph in nodes glyphstring that we render */
  guint                num_glyphs; /* number of *non-empty* glyphs (== instances) we render */
  float                scale;
};

struct _GskVulkanOpPushConstants
{
  GskVulkanOpType         type;
  GskRenderNode          *node; /* node that's the source of this op */
  GskVulkanPushConstants  constants; /* new constants to push */
};

union _GskVulkanOp
{
  GskVulkanOpType          type;
  GskVulkanOpRender        render;
  GskVulkanOpText          text;
  GskVulkanOpPushConstants constants;
};

struct _GskVulkanRenderPass
{
  GdkVulkanContext *vulkan;

  GArray *render_ops;

  GskVulkanImage *target;
  graphene_rect_t viewport;
  cairo_region_t *clip;

  graphene_vec2_t scale;
  graphene_point_t offset;

  VkRenderPass render_pass;
  VkSemaphore signal_semaphore;
  GArray *wait_semaphores;
  GskVulkanBuffer *vertex_data;
};

#ifdef G_ENABLE_DEBUG
static GQuark fallback_pixels_quark;
static GQuark texture_pixels_quark;
#endif

GskVulkanRenderPass *
gsk_vulkan_render_pass_new (GdkVulkanContext      *context,
                            GskVulkanImage        *target,
                            const graphene_vec2_t *scale,
                            const graphene_rect_t *viewport,
                            cairo_region_t        *clip,
                            VkSemaphore            signal_semaphore)
{
  GskVulkanRenderPass *self;
  VkImageLayout final_layout;

  self = g_new0 (GskVulkanRenderPass, 1);
  self->vulkan = g_object_ref (context);
  self->render_ops = g_array_new (FALSE, FALSE, sizeof (GskVulkanOp));

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
                                              .format = gdk_vulkan_context_get_image_format (self->vulkan),
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

  return self;
}

void
gsk_vulkan_render_pass_free (GskVulkanRenderPass *self)
{
  g_array_unref (self->render_ops);
  g_object_unref (self->vulkan);
  g_object_unref (self->target);
  cairo_region_destroy (self->clip);
  vkDestroyRenderPass (gdk_vulkan_context_get_device (self->vulkan),
                       self->render_pass,
                       NULL);
  if (self->vertex_data)
    gsk_vulkan_buffer_free (self->vertex_data);
  if (self->signal_semaphore != VK_NULL_HANDLE)
    vkDestroySemaphore (gdk_vulkan_context_get_device (self->vulkan),
                        self->signal_semaphore,
                        NULL);
  g_array_unref (self->wait_semaphores);

  g_free (self);
}

#define FALLBACK(...) G_STMT_START { \
  GSK_RENDERER_DEBUG (gsk_vulkan_render_get_renderer (render), FALLBACK, __VA_ARGS__); \
  return FALSE; \
}G_STMT_END

static void
gsk_vulkan_render_pass_add_node (GskVulkanRenderPass           *self,
                                 GskVulkanRender               *render,
                                 const GskVulkanPushConstants  *constants,
                                 GskRenderNode                 *node);

static inline gboolean
gsk_vulkan_render_pass_add_fallback_node (GskVulkanRenderPass          *self,
                                          GskVulkanRender              *render,
                                          const GskVulkanPushConstants *constants,
                                          GskRenderNode                *node)
{
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  switch (constants->clip.type)
    {
      case GSK_VULKAN_CLIP_NONE:
        op.type = GSK_VULKAN_OP_FALLBACK;
        break;
      case GSK_VULKAN_CLIP_RECT:
        op.type = GSK_VULKAN_OP_FALLBACK_CLIP;
        gsk_rounded_rect_init_copy (&op.render.clip, &constants->clip.rect);
        break;
      case GSK_VULKAN_CLIP_ROUNDED_CIRCULAR:
      case GSK_VULKAN_CLIP_ROUNDED:
        op.type = GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP;
        gsk_rounded_rect_init_copy (&op.render.clip, &constants->clip.rect);
        break;
      case GSK_VULKAN_CLIP_ALL_CLIPPED:
      default:
        g_assert_not_reached ();
        return FALSE;
    }

  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_TEXTURE);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_implode (GskVulkanRenderPass          *self,
                                GskVulkanRender              *render,
                                const GskVulkanPushConstants *constants,
                                GskRenderNode                *node)
{
  g_assert_not_reached ();
  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_container_node (GskVulkanRenderPass          *self,
                                           GskVulkanRender              *render,
                                           const GskVulkanPushConstants *constants,
                                           GskRenderNode                *node)
{
  for (guint i = 0; i < gsk_container_node_get_n_children (node); i++)
    gsk_vulkan_render_pass_add_node (self, render, constants, gsk_container_node_get_child (node, i));

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_cairo_node (GskVulkanRenderPass          *self,
                                       GskVulkanRender              *render,
                                       const GskVulkanPushConstants *constants,
                                       GskRenderNode                *node)
{
  /* We're using recording surfaces, so drawing them to an image
   * surface and uploading them is the right thing.
   * But that's exactly what the fallback code does.
   */
  if (gsk_cairo_node_get_surface (node) != NULL)
    return gsk_vulkan_render_pass_add_fallback_node (self, render, constants, node);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_color_node (GskVulkanRenderPass          *self,
                                       GskVulkanRender              *render,
                                       const GskVulkanPushConstants *constants,
                                       GskRenderNode                *node)
{
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };
  GskVulkanPipelineType pipeline_type;

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_COLOR;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_COLOR_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_COLOR_CLIP_ROUNDED;
  else
    FALLBACK ("Color nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_COLOR;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_repeating_linear_gradient_node (GskVulkanRenderPass          *self,
                                                           GskVulkanRender              *render,
                                                           const GskVulkanPushConstants *constants,
                                                           GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (gsk_linear_gradient_node_get_n_color_stops (node) > GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS)
    FALLBACK ("Linear gradient with %zu color stops, hardcoded limit is %u",
              gsk_linear_gradient_node_get_n_color_stops (node),
              GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS);

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_LINEAR_GRADIENT;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_LINEAR_GRADIENT_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_LINEAR_GRADIENT_CLIP_ROUNDED;
  else
    FALLBACK ("Linear gradient nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_LINEAR_GRADIENT;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_border_node (GskVulkanRenderPass          *self,
                                        GskVulkanRender              *render,
                                        const GskVulkanPushConstants *constants,
                                        GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_BORDER;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_BORDER_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_BORDER_CLIP_ROUNDED;
  else
    FALLBACK ("Border nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_BORDER;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_texture_node (GskVulkanRenderPass          *self,
                                         GskVulkanRender              *render,
                                         const GskVulkanPushConstants *constants,
                                         GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_TEXTURE;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_TEXTURE_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_TEXTURE_CLIP_ROUNDED;
  else
    FALLBACK ("Texture nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_TEXTURE;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_inset_shadow_node (GskVulkanRenderPass          *self,
                                              GskVulkanRender              *render,
                                              const GskVulkanPushConstants *constants,
                                              GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (gsk_inset_shadow_node_get_blur_radius (node) > 0)
    FALLBACK ("Blur support not implemented for inset shadows");
  else if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_INSET_SHADOW;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_INSET_SHADOW_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_INSET_SHADOW_CLIP_ROUNDED;
  else
    FALLBACK ("Inset shadow nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_INSET_SHADOW;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_outset_shadow_node (GskVulkanRenderPass          *self,
                                               GskVulkanRender              *render,
                                               const GskVulkanPushConstants *constants,
                                               GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (gsk_outset_shadow_node_get_blur_radius (node) > 0)
    FALLBACK ("Blur support not implemented for outset shadows");
  else if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_OUTSET_SHADOW;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_OUTSET_SHADOW_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_OUTSET_SHADOW_CLIP_ROUNDED;
  else
    FALLBACK ("Outset shadow nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_OUTSET_SHADOW;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_transform_node (GskVulkanRenderPass          *self,
                                           GskVulkanRender              *render,
                                           const GskVulkanPushConstants *constants,
                                           GskRenderNode                *node)
{
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };
  GskRenderNode *child;
  GskTransform *transform;
  graphene_vec2_t old_scale;
  graphene_point_t old_offset;
  float scale_x;
  float scale_y;
  graphene_vec2_t scale;

#if 0
 if (!gsk_vulkan_clip_contains_rect (clip, &node->bounds))
    FALLBACK ("Transform nodes can't deal with clip type %u\n", clip->type);
#endif

  child = gsk_transform_node_get_child (node);
  transform = gsk_transform_node_get_transform (node);

  switch (gsk_transform_get_category (transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;
        gsk_transform_to_translate (transform, &dx, &dy);
        self->offset.x += dx;
        self->offset.y += dy;
        gsk_vulkan_render_pass_add_node (self, render, constants, child);
        self->offset.x -= dx;
        self->offset.y -= dy;
      }
      return TRUE;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float dx, dy;
        gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
      }
      break;

    case GSK_TRANSFORM_CATEGORY_2D:
      {
        float xx, xy, yx, yy, dx, dy;

        gsk_transform_to_2d (transform,
                             &xx, &xy, &yx, &yy, &dx, &dy);

        scale_x = sqrtf (xx * xx + xy * xy);
        scale_y = sqrtf (yx * yx + yy * yy);
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

        gsk_transform_to_matrix (transform, &matrix);
        graphene_matrix_decompose (&matrix,
                                   &translation,
                                   &matrix_scale,
                                   &rotation,
                                   &shear,
                                   &perspective);

        scale_x = graphene_vec3_get_x (&matrix_scale);
        scale_y = graphene_vec3_get_y (&matrix_scale);
      }
      break;

    default:
      break;
    }

  transform = gsk_transform_transform (gsk_transform_translate (NULL, &self->offset),
                                       transform);
  if (!gsk_vulkan_push_constants_transform (&op.constants.constants, constants, transform, &child->bounds))
    FALLBACK ("Transform nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
  g_array_append_val (self->render_ops, op);

  old_offset = self->offset;
  self->offset = *graphene_point_zero ();
  graphene_vec2_init_from_vec2 (&old_scale, &self->scale);
  graphene_vec2_init (&scale, fabs (scale_x), fabs (scale_y));
  graphene_vec2_multiply (&self->scale, &scale, &self->scale);

  gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, child);

  gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
  g_array_append_val (self->render_ops, op);

  graphene_vec2_init_from_vec2 (&self->scale, &old_scale);
  self->offset = old_offset;

  gsk_transform_unref (transform);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_opacity_node (GskVulkanRenderPass          *self,
                                         GskVulkanRender              *render,
                                         const GskVulkanPushConstants *constants,
                                         GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_COLOR_MATRIX;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_COLOR_MATRIX_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_COLOR_MATRIX_CLIP_ROUNDED;
  else
    FALLBACK ("Opacity nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_OPACITY;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_color_matrix_node (GskVulkanRenderPass          *self,
                                              GskVulkanRender              *render,
                                              const GskVulkanPushConstants *constants,
                                              GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_COLOR_MATRIX;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_COLOR_MATRIX_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_COLOR_MATRIX_CLIP_ROUNDED;
  else
    FALLBACK ("Color matrix nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_COLOR_MATRIX;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_clip_node (GskVulkanRenderPass          *self,
                                      GskVulkanRender              *render,
                                      const GskVulkanPushConstants *constants,
                                      GskRenderNode                *node)
{
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };
  graphene_rect_t clip;

  graphene_rect_offset_r (gsk_clip_node_get_clip (node),
                          self->offset.x, self->offset.y,
                          &clip);

  if (!gsk_vulkan_push_constants_intersect_rect (&op.constants.constants, constants, &clip))
    FALLBACK ("Failed to find intersection between clip of type %u and rectangle", constants->clip.type);

  if (op.constants.constants.clip.type == GSK_VULKAN_CLIP_ALL_CLIPPED)
    return TRUE;

  op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
  g_array_append_val (self->render_ops, op);

  gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, gsk_clip_node_get_child (node));

  gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_rounded_clip_node (GskVulkanRenderPass          *self,
                                              GskVulkanRender              *render,
                                              const GskVulkanPushConstants *constants,
                                              GskRenderNode                *node)
{
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };
  GskRoundedRect clip;

  clip = *gsk_rounded_clip_node_get_clip (node);
  gsk_rounded_rect_offset (&clip, self->offset.x, self->offset.y);

  if (!gsk_vulkan_push_constants_intersect_rounded (&op.constants.constants, constants, &clip))
    FALLBACK ("Failed to find intersection between clip of type %u and rounded rectangle", constants->clip.type);

  if (op.constants.constants.clip.type == GSK_VULKAN_CLIP_ALL_CLIPPED)
    return TRUE;

  op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
  g_array_append_val (self->render_ops, op);

  gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, gsk_rounded_clip_node_get_child (node));

  gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_repeat_node (GskVulkanRenderPass          *self,
                                        GskVulkanRender              *render,
                                        const GskVulkanPushConstants *constants,
                                        GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (graphene_rect_get_area (gsk_repeat_node_get_child_bounds (node)) == 0)
    return TRUE;

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_TEXTURE;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_TEXTURE_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_TEXTURE_CLIP_ROUNDED;
  else
    FALLBACK ("Repeat nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_REPEAT;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_blend_node (GskVulkanRenderPass          *self,
                                       GskVulkanRender              *render,
                                       const GskVulkanPushConstants *constants,
                                       GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_BLEND_MODE;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_BLEND_MODE_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_BLEND_MODE_CLIP_ROUNDED;
  else
    FALLBACK ("Blend nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_BLEND_MODE;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_cross_fade_node (GskVulkanRenderPass          *self,
                                            GskVulkanRender              *render,
                                            const GskVulkanPushConstants *constants,
                                            GskRenderNode                *node)
{
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };
  GskVulkanPipelineType pipeline_type;

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_CROSS_FADE;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_CROSS_FADE_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_CROSS_FADE_CLIP_ROUNDED;
  else
    FALLBACK ("Cross fade nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_CROSS_FADE;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_text_node (GskVulkanRenderPass          *self,
                                      GskVulkanRender              *render,
                                      const GskVulkanPushConstants *constants,
                                      GskRenderNode                *node)
{
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };
  GskVulkanPipelineType pipeline_type;
  const PangoGlyphInfo *glyphs;
  GskVulkanRenderer *renderer;
  const PangoFont *font;
  guint texture_index;
  guint num_glyphs;
  guint count;
  int x_position;
  int i;

  renderer = GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render));
  num_glyphs = gsk_text_node_get_num_glyphs (node);
  glyphs = gsk_text_node_get_glyphs (node, NULL);
  font = gsk_text_node_get_font (node);

  if (gsk_text_node_has_color_glyphs (node))
    {
      if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
        pipeline_type = GSK_VULKAN_PIPELINE_COLOR_TEXT;
      else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
        pipeline_type = GSK_VULKAN_PIPELINE_COLOR_TEXT_CLIP;
      else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
        pipeline_type = GSK_VULKAN_PIPELINE_COLOR_TEXT_CLIP_ROUNDED;
      else
        FALLBACK ("Text nodes can't deal with clip type %u", constants->clip.type);
      op.type = GSK_VULKAN_OP_COLOR_TEXT;
    }
  else
    {
      if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
        pipeline_type = GSK_VULKAN_PIPELINE_TEXT;
      else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
        pipeline_type = GSK_VULKAN_PIPELINE_TEXT_CLIP;
      else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
        pipeline_type = GSK_VULKAN_PIPELINE_TEXT_CLIP_ROUNDED;
      else
        FALLBACK ("Text nodes can't deal with clip type %u", constants->clip.type);
      op.type = GSK_VULKAN_OP_TEXT;
    }
  op.text.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);

  op.text.start_glyph = 0;
  op.text.texture_index = G_MAXUINT;
  op.text.scale = MAX (graphene_vec2_get_x (&self->scale), graphene_vec2_get_y (&self->scale));

  x_position = 0;
  for (i = 0, count = 0; i < num_glyphs; i++)
    {
      const PangoGlyphInfo *gi = &glyphs[i];

      texture_index = gsk_vulkan_renderer_cache_glyph (renderer,
                                                       (PangoFont *)font,
                                                       gi->glyph,
                                                       x_position + gi->geometry.x_offset,
                                                       gi->geometry.y_offset,
                                                       op.text.scale);
      if (op.text.texture_index == G_MAXUINT)
        op.text.texture_index = texture_index;
      if (texture_index != op.text.texture_index)
        {
          op.text.num_glyphs = count;

          g_array_append_val (self->render_ops, op);

          count = 1;
          op.text.start_glyph = i;
          op.text.texture_index = texture_index;
        }
      else
        count++;

      x_position += gi->geometry.width;
    }

  if (op.text.texture_index != G_MAXUINT && count != 0)
    {
      op.text.num_glyphs = count;
      g_array_append_val (self->render_ops, op);
    }

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_blur_node (GskVulkanRenderPass          *self,
                                      GskVulkanRender              *render,
                                      const GskVulkanPushConstants *constants,
                                      GskRenderNode                *node)
{
  GskVulkanPipelineType pipeline_type;
  GskVulkanOp op = {
    .render.node = node,
    .render.offset = self->offset,
  };

  if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
    pipeline_type = GSK_VULKAN_PIPELINE_BLUR;
  else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
    pipeline_type = GSK_VULKAN_PIPELINE_BLUR_CLIP;
  else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
    pipeline_type = GSK_VULKAN_PIPELINE_BLUR_CLIP_ROUNDED;
  else
    FALLBACK ("Blur nodes can't deal with clip type %u", constants->clip.type);

  op.type = GSK_VULKAN_OP_BLUR;
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
  g_array_append_val (self->render_ops, op);

  return TRUE;
}

static inline gboolean
gsk_vulkan_render_pass_add_debug_node (GskVulkanRenderPass          *self,
                                       GskVulkanRender              *render,
                                       const GskVulkanPushConstants *constants,
                                       GskRenderNode                *node)
{
  gsk_vulkan_render_pass_add_node (self, render, constants, gsk_debug_node_get_child (node));
  return TRUE;
}

#undef FALLBACK

typedef gboolean (*GskVulkanRenderPassNodeFunc) (GskVulkanRenderPass          *self,
                                                 GskVulkanRender              *render,
                                                 const GskVulkanPushConstants *constants,
                                                 GskRenderNode                *node);

#define N_RENDER_NODES (GSK_MASK_NODE + 1)

/* TODO: implement remaining nodes */
static const GskVulkanRenderPassNodeFunc nodes_vtable[N_RENDER_NODES] = {
  [GSK_NOT_A_RENDER_NODE] = gsk_vulkan_render_pass_implode,
  [GSK_CONTAINER_NODE] = gsk_vulkan_render_pass_add_container_node,
  [GSK_CAIRO_NODE] = gsk_vulkan_render_pass_add_cairo_node,
  [GSK_COLOR_NODE] = gsk_vulkan_render_pass_add_color_node,
  [GSK_LINEAR_GRADIENT_NODE] = gsk_vulkan_render_pass_add_repeating_linear_gradient_node,
  [GSK_REPEATING_LINEAR_GRADIENT_NODE] = gsk_vulkan_render_pass_add_repeating_linear_gradient_node,
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
  [GSK_TEXTURE_SCALE_NODE] = NULL,
  [GSK_MASK_NODE] = NULL,
};

static void
gsk_vulkan_render_pass_add_node (GskVulkanRenderPass           *self,
                                 GskVulkanRender               *render,
                                 const GskVulkanPushConstants  *constants,
                                 GskRenderNode                 *node)
{
  GskVulkanRenderPassNodeFunc node_func;
  GskRenderNodeType node_type;
  gboolean fallback = FALSE;

  node_type = gsk_render_node_get_node_type (node);
  node_func = nodes_vtable[node_type];

  if (node_func)
    {
      if (!node_func (self, render, constants, node))
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
    gsk_vulkan_render_pass_add_fallback_node (self, render, constants, node);
}

void
gsk_vulkan_render_pass_add (GskVulkanRenderPass     *self,
                            GskVulkanRender         *render,
                            GskRenderNode           *node)
{
  GskVulkanOp op = { 0, };
  graphene_matrix_t projection, mvp;

  graphene_matrix_init_scale (&mvp,
                              graphene_vec2_get_x (&self->scale),
                              graphene_vec2_get_y (&self->scale),
                              1.0);
  graphene_matrix_init_ortho (&projection,
                              self->viewport.origin.x, self->viewport.origin.x + self->viewport.size.width,
                              self->viewport.origin.y, self->viewport.origin.y + self->viewport.size.height,
                              2 * ORTHO_NEAR_PLANE - ORTHO_FAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_matrix_multiply (&mvp, &projection, &mvp);

  op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
  gsk_vulkan_push_constants_init (&op.constants.constants, &mvp, &self->viewport);
  g_array_append_val (self->render_ops, op);

  gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, node);

  self->offset = GRAPHENE_POINT_INIT (0, 0);
}

static GskVulkanImage *
gsk_vulkan_render_pass_render_offscreen (GdkVulkanContext      *vulkan,
                                         GskVulkanRender       *render,
                                         GskVulkanUploader     *uploader,
                                         VkSemaphore            semaphore,
                                         GskRenderNode         *node,
                                         const graphene_vec2_t *scale,
                                         const graphene_rect_t *viewport)
{
  graphene_rect_t view;
  cairo_region_t *clip;
  GskVulkanRenderPass *pass;
  GskVulkanImage *result;
  float scale_x, scale_y;

  scale_x = graphene_vec2_get_x (scale);
  scale_y = graphene_vec2_get_y (scale);
  view = GRAPHENE_RECT_INIT (viewport->origin.x,
                             viewport->origin.y,
                             ceil (scale_x * viewport->size.width),
                             ceil (scale_y * viewport->size.height));

  result = gsk_vulkan_image_new_for_offscreen (vulkan,
                                               view.size.width, view.size.height);

#ifdef G_ENABLE_DEBUG
  {
    GskProfiler *profiler = gsk_renderer_get_profiler (gsk_vulkan_render_get_renderer (render));
    gsk_profiler_counter_add (profiler,
                              texture_pixels_quark,
                              view.size.width * view.size.height);
  }
#endif

  clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                          0, 0,
                                          gsk_vulkan_image_get_width (result),
                                          gsk_vulkan_image_get_height (result)
                                        });

  pass = gsk_vulkan_render_pass_new (vulkan,
                                     result,
                                     scale,
                                     &view,
                                     clip,
                                     semaphore);

  cairo_region_destroy (clip);

  gsk_vulkan_render_add_render_pass (render, pass);
  gsk_vulkan_render_pass_add (pass, render, node);
  gsk_vulkan_render_add_cleanup_image (render, result);

  return result;
}

static GskVulkanImage *
gsk_vulkan_render_pass_get_node_as_texture (GskVulkanRenderPass    *self,
                                            GskVulkanRender        *render,
                                            GskVulkanUploader      *uploader,
                                            GskRenderNode          *node,
                                            const graphene_rect_t  *clip_bounds,
                                            const graphene_point_t *clip_offset,
                                            graphene_rect_t        *tex_bounds)
{
  VkSemaphore semaphore;
  GskVulkanImage *result;
  cairo_surface_t *surface;
  cairo_t *cr;

  switch ((guint) gsk_render_node_get_node_type (node))
    {
    case GSK_TEXTURE_NODE:
      result = gsk_vulkan_renderer_ref_texture_image (GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render)),
                                                      gsk_texture_node_get_texture (node),
                                                      uploader);
      gsk_vulkan_render_add_cleanup_image (render, result);
      *tex_bounds = node->bounds;
      return result;

    case GSK_CAIRO_NODE:
      /* We're using recording surfaces, so drawing them to an image
       * surface and uploading them is the right thing.
       * But that's exactly what the fallback code does.
       */
      break;

    default:
      {
        graphene_rect_t clipped;

        graphene_rect_offset_r (clip_bounds, - clip_offset->x, - clip_offset->y, &clipped);
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

        return gsk_vulkan_render_pass_render_offscreen (self->vulkan,
                                                        render,
                                                        uploader,
                                                        semaphore,
                                                        node,
                                                        &self->scale,
                                                        &clipped);
      }
   }

  GSK_RENDERER_DEBUG (gsk_vulkan_render_get_renderer (render), FALLBACK, "Node as texture not implemented for this case. Using %gx%g fallback surface",
                      ceil (node->bounds.size.width),
                      ceil (node->bounds.size.height));
#ifdef G_ENABLE_DEBUG
  {
    GskProfiler *profiler = gsk_renderer_get_profiler (gsk_vulkan_render_get_renderer (render));
    gsk_profiler_counter_add (profiler,
                              fallback_pixels_quark,
                              ceil (node->bounds.size.width) * ceil (node->bounds.size.height));
  }
#endif

  /* XXX: We could intersect bounds with clip bounds here */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        ceil (node->bounds.size.width),
                                        ceil (node->bounds.size.height));
  cr = cairo_create (surface);
  cairo_translate (cr, -node->bounds.origin.x, -node->bounds.origin.y);

  gsk_render_node_draw (node, cr);

  cairo_destroy (cr);

  result = gsk_vulkan_image_new_from_data (uploader,
                                           cairo_image_surface_get_data (surface),
                                           cairo_image_surface_get_width (surface),
                                           cairo_image_surface_get_height (surface),
                                           cairo_image_surface_get_stride (surface));

  cairo_surface_destroy (surface);

  gsk_vulkan_render_add_cleanup_image (render, result);

  *tex_bounds = node->bounds;

  return result;
}

static void
gsk_vulkan_render_pass_upload_fallback (GskVulkanRenderPass  *self,
                                        GskVulkanOpRender    *op,
                                        GskVulkanRender      *render,
                                        GskVulkanUploader    *uploader)
{
  GskRenderNode *node;
  cairo_surface_t *surface;
  cairo_t *cr;
  float scale_x, scale_y;

  node = op->node;

  GSK_RENDERER_DEBUG (gsk_vulkan_render_get_renderer (render), FALLBACK,
                      "Upload op=%s, node %s[%p], bounds %gx%g",
                      op->type == GSK_VULKAN_OP_FALLBACK_CLIP ? "fallback-clip" :
                      (op->type == GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP ? "fallback-rounded-clip" : "fallback"),
                      g_type_name_from_instance ((GTypeInstance *) node), node,
                      ceil (node->bounds.size.width),
                      ceil (node->bounds.size.height));
#ifdef G_ENABLE_DEBUG
  {
    GskProfiler *profiler = gsk_renderer_get_profiler (gsk_vulkan_render_get_renderer (render));
    gsk_profiler_counter_add (profiler,
                              fallback_pixels_quark,
                              ceil (node->bounds.size.width) * ceil (node->bounds.size.height));
  }
#endif

  scale_x = graphene_vec2_get_x (&self->scale);
  scale_y = graphene_vec2_get_y (&self->scale);
  /* XXX: We could intersect bounds with clip bounds here */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        ceil (node->bounds.size.width * scale_x),
                                        ceil (node->bounds.size.height * scale_y));
  cairo_surface_set_device_scale (surface, scale_x, scale_y);
  cr = cairo_create (surface);
  cairo_translate (cr, -node->bounds.origin.x, -node->bounds.origin.y);

  if (op->type == GSK_VULKAN_OP_FALLBACK_CLIP)
    {
      cairo_rectangle (cr,
                       op->clip.bounds.origin.x - op->offset.x,
                       op->clip.bounds.origin.y - op->offset.y,
                       op->clip.bounds.size.width,
                       op->clip.bounds.size.height);
      cairo_clip (cr);
    }
  else if (op->type == GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP)
    {
      cairo_translate (cr, - op->offset.x, - op->offset.y);
      gsk_rounded_rect_path (&op->clip, cr);
      cairo_translate (cr, op->offset.x, op->offset.y);
      cairo_clip (cr);
    }
  else
    {
      g_assert (op->type == GSK_VULKAN_OP_FALLBACK);
    }

  gsk_render_node_draw (node, cr);

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (gsk_vulkan_render_get_renderer (render), FALLBACK))
    {
      cairo_rectangle (cr,
                       op->clip.bounds.origin.x - op->offset.x,
                       op->clip.bounds.origin.y - op->offset.y,
                       op->clip.bounds.size.width,
                       op->clip.bounds.size.height);
      if (gsk_render_node_get_node_type (node) == GSK_CAIRO_NODE)
        cairo_set_source_rgba (cr, 0.3, 0, 1, 0.25);
      else
        cairo_set_source_rgba (cr, 1, 0, 0, 0.25);
      cairo_fill_preserve (cr);
      if (gsk_render_node_get_node_type (node) == GSK_CAIRO_NODE)
        cairo_set_source_rgba (cr, 0.3, 0, 1, 1);
      else
        cairo_set_source_rgba (cr, 1, 0, 0, 1);
      cairo_stroke (cr);
    }
#endif

  cairo_destroy (cr);

  op->source = gsk_vulkan_image_new_from_data (uploader,
                                               cairo_image_surface_get_data (surface),
                                               cairo_image_surface_get_width (surface),
                                               cairo_image_surface_get_height (surface),
                                               cairo_image_surface_get_stride (surface));

  op->source_rect = GRAPHENE_RECT_INIT(0, 0, 1, 1);

  cairo_surface_destroy (surface);

  gsk_vulkan_render_add_cleanup_image (render, op->source);
}

static void
get_tex_rect (graphene_rect_t       *tex_coords,
              const graphene_rect_t *rect,
              const graphene_rect_t *tex)
{
  graphene_rect_init (tex_coords,
                      (rect->origin.x - tex->origin.x) / tex->size.width,
                      (rect->origin.y - tex->origin.y) / tex->size.height,
                      rect->size.width / tex->size.width,
                      rect->size.height / tex->size.height);
}

void
gsk_vulkan_render_pass_upload (GskVulkanRenderPass  *self,
                               GskVulkanRender      *render,
                               GskVulkanUploader    *uploader)
{
  GskVulkanOp *op;
  guint i;
  const graphene_rect_t *clip = NULL;

  for (i = 0; i < self->render_ops->len; i++)
    {
      op = &g_array_index (self->render_ops, GskVulkanOp, i);

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
        case GSK_VULKAN_OP_FALLBACK_CLIP:
        case GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP:
          gsk_vulkan_render_pass_upload_fallback (self, &op->render, render, uploader);
          break;

        case GSK_VULKAN_OP_TEXT:
        case GSK_VULKAN_OP_COLOR_TEXT:
          {
            op->text.source = gsk_vulkan_renderer_ref_glyph_image (GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render)),
                                                                   uploader,
                                                                   op->text.texture_index);
            gsk_vulkan_render_add_cleanup_image (render, op->text.source);
          }
          break;

        case GSK_VULKAN_OP_TEXTURE:
          {
            op->render.source = gsk_vulkan_renderer_ref_texture_image (GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render)),
                                                                       gsk_texture_node_get_texture (op->render.node),
                                                                       uploader);
            op->render.source_rect = GRAPHENE_RECT_INIT(0, 0, 1, 1);
            gsk_vulkan_render_add_cleanup_image (render, op->render.source);
          }
          break;

        case GSK_VULKAN_OP_OPACITY:
          {
            GskRenderNode *child = gsk_opacity_node_get_child (op->render.node);
            graphene_rect_t tex_bounds;

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            child,
                                                                            clip,
                                                                            &op->render.offset,
                                                                            &tex_bounds);
            get_tex_rect (&op->render.source_rect, &op->render.node->bounds, &tex_bounds);
          }
          break;

        case GSK_VULKAN_OP_REPEAT:
          {
            GskRenderNode *child = gsk_repeat_node_get_child (op->render.node);
            const graphene_rect_t *child_bounds = gsk_repeat_node_get_child_bounds (op->render.node);
            graphene_rect_t tex_bounds;

            if (!graphene_rect_equal (child_bounds, &child->bounds))
              {
                VkSemaphore semaphore;

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

                op->render.source = gsk_vulkan_render_pass_render_offscreen (self->vulkan,
                                                                             render,
                                                                             uploader,
                                                                             semaphore,
                                                                             child,
                                                                             &self->scale,
                                                                             child_bounds);
                get_tex_rect (&op->render.source_rect, &op->render.node->bounds, child_bounds);
              }
            else
              {
                op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                                render,
                                                                                uploader,
                                                                                child,
                                                                                &child->bounds,
                                                                                &GRAPHENE_POINT_INIT (0, 0),
                                                                                &tex_bounds);
                get_tex_rect (&op->render.source_rect, &op->render.node->bounds, &tex_bounds);
              }
          }
          break;

        case GSK_VULKAN_OP_BLUR:
          {
            GskRenderNode *child = gsk_blur_node_get_child (op->render.node);
            graphene_rect_t tex_bounds;

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            child,
                                                                            clip,
                                                                            &op->render.offset,
                                                                            &tex_bounds);
            get_tex_rect (&op->render.source_rect, &op->render.node->bounds, &tex_bounds);
          }
          break;

        case GSK_VULKAN_OP_COLOR_MATRIX:
          {
            GskRenderNode *child = gsk_color_matrix_node_get_child (op->render.node);
            graphene_rect_t tex_bounds;

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            child,
                                                                            clip,
                                                                            &op->render.offset,
                                                                            &tex_bounds);
            get_tex_rect (&op->render.source_rect, &op->render.node->bounds, &tex_bounds);
          }
          break;

        case GSK_VULKAN_OP_CROSS_FADE:
          {
            GskRenderNode *start = gsk_cross_fade_node_get_start_child (op->render.node);
            GskRenderNode *end = gsk_cross_fade_node_get_end_child (op->render.node);
            graphene_rect_t tex_bounds;

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            start,
                                                                            clip,
                                                                            &op->render.offset,
                                                                            &tex_bounds);
            get_tex_rect (&op->render.source_rect, &op->render.node->bounds, &tex_bounds);

            op->render.source2 = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                             render,
                                                                             uploader,
                                                                             end,
                                                                             clip,
                                                                             &op->render.offset,
                                                                             &tex_bounds);
            get_tex_rect (&op->render.source2_rect, &op->render.node->bounds, &tex_bounds);
          }
          break;

        case GSK_VULKAN_OP_BLEND_MODE:
          {
            GskRenderNode *top = gsk_blend_node_get_top_child (op->render.node);
            GskRenderNode *bottom = gsk_blend_node_get_bottom_child (op->render.node);
            graphene_rect_t tex_bounds;

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            top,
                                                                            clip,
                                                                            &op->render.offset,
                                                                            &tex_bounds);
            get_tex_rect (&op->render.source_rect, &op->render.node->bounds, &tex_bounds);

            op->render.source2 = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                             render,
                                                                             uploader,
                                                                             bottom,
                                                                             clip,
                                                                             &op->render.offset,
                                                                             &tex_bounds);
            get_tex_rect (&op->render.source2_rect, &op->render.node->bounds, &tex_bounds);
          }
          break;

        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
          clip = &op->constants.constants.clip.rect.bounds;
          break;

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_COLOR:
        case GSK_VULKAN_OP_LINEAR_GRADIENT:
        case GSK_VULKAN_OP_BORDER:
        case GSK_VULKAN_OP_INSET_SHADOW:
        case GSK_VULKAN_OP_OUTSET_SHADOW:
          break;
        }
    }
}

static gsize
gsk_vulkan_render_pass_count_vertex_data (GskVulkanRenderPass *self)
{
  GskVulkanOp *op;
  gsize n_bytes;
  guint i;

  n_bytes = 0;
  for (i = 0; i < self->render_ops->len; i++)
    {
      op = &g_array_index (self->render_ops, GskVulkanOp, i);

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
        case GSK_VULKAN_OP_FALLBACK_CLIP:
        case GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP:
        case GSK_VULKAN_OP_TEXTURE:
        case GSK_VULKAN_OP_REPEAT:
          op->render.vertex_count = gsk_vulkan_texture_pipeline_count_vertex_data (GSK_VULKAN_TEXTURE_PIPELINE (op->render.pipeline));
          n_bytes += op->render.vertex_count;
          break;

        case GSK_VULKAN_OP_TEXT:
          op->text.vertex_count = gsk_vulkan_text_pipeline_count_vertex_data (GSK_VULKAN_TEXT_PIPELINE (op->text.pipeline),
                                                                              op->text.num_glyphs);
          n_bytes += op->text.vertex_count;
          break;

        case GSK_VULKAN_OP_COLOR_TEXT:
          op->text.vertex_count = gsk_vulkan_color_text_pipeline_count_vertex_data (GSK_VULKAN_COLOR_TEXT_PIPELINE (op->render.pipeline),
                                                                                    op->text.num_glyphs);
          n_bytes += op->text.vertex_count;
          break;

        case GSK_VULKAN_OP_COLOR:
          op->render.vertex_count = gsk_vulkan_color_pipeline_count_vertex_data (GSK_VULKAN_COLOR_PIPELINE (op->render.pipeline));
          n_bytes += op->render.vertex_count;
          break;

        case GSK_VULKAN_OP_LINEAR_GRADIENT:
          op->render.vertex_count = gsk_vulkan_linear_gradient_pipeline_count_vertex_data (GSK_VULKAN_LINEAR_GRADIENT_PIPELINE (op->render.pipeline));
          n_bytes += op->render.vertex_count;
          break;

        case GSK_VULKAN_OP_OPACITY:
        case GSK_VULKAN_OP_COLOR_MATRIX:
          op->render.vertex_count = gsk_vulkan_effect_pipeline_count_vertex_data (GSK_VULKAN_EFFECT_PIPELINE (op->render.pipeline));
          n_bytes += op->render.vertex_count;
          break;

        case GSK_VULKAN_OP_BLUR:
          op->render.vertex_count = gsk_vulkan_blur_pipeline_count_vertex_data (GSK_VULKAN_BLUR_PIPELINE (op->render.pipeline));
          n_bytes += op->render.vertex_count;
          break;

        case GSK_VULKAN_OP_BORDER:
          op->render.vertex_count = gsk_vulkan_border_pipeline_count_vertex_data (GSK_VULKAN_BORDER_PIPELINE (op->render.pipeline));
          n_bytes += op->render.vertex_count;
          break;

        case GSK_VULKAN_OP_INSET_SHADOW:
        case GSK_VULKAN_OP_OUTSET_SHADOW:
          op->render.vertex_count = gsk_vulkan_box_shadow_pipeline_count_vertex_data (GSK_VULKAN_BOX_SHADOW_PIPELINE (op->render.pipeline));
          n_bytes += op->render.vertex_count;
          break;

        case GSK_VULKAN_OP_CROSS_FADE:
          op->render.vertex_count = gsk_vulkan_cross_fade_pipeline_count_vertex_data (GSK_VULKAN_CROSS_FADE_PIPELINE (op->render.pipeline));
          n_bytes += op->render.vertex_count;
          break;

        case GSK_VULKAN_OP_BLEND_MODE:
          op->render.vertex_count = gsk_vulkan_blend_mode_pipeline_count_vertex_data (GSK_VULKAN_BLEND_MODE_PIPELINE (op->render.pipeline));
          n_bytes += op->render.vertex_count;
          break;

        default:
          g_assert_not_reached ();

        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
          continue;
        }
    }

  return n_bytes;
}

static gsize
gsk_vulkan_render_pass_collect_vertex_data (GskVulkanRenderPass *self,
                                            GskVulkanRender     *render,
                                            guchar              *data,
                                            gsize                offset,
                                            gsize                total)
{
  GskVulkanOp *op;
  gsize n_bytes;
  guint i;

  n_bytes = 0;
  for (i = 0; i < self->render_ops->len; i++)
    {
      op = &g_array_index (self->render_ops, GskVulkanOp, i);

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
        case GSK_VULKAN_OP_FALLBACK_CLIP:
        case GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP:
        case GSK_VULKAN_OP_TEXTURE:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_texture_pipeline_collect_vertex_data (GSK_VULKAN_TEXTURE_PIPELINE (op->render.pipeline),
                                                             data + n_bytes + offset,
                                                             &op->render.offset,
                                                             &op->render.node->bounds,
                                                             &op->render.source_rect);
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_REPEAT:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_texture_pipeline_collect_vertex_data (GSK_VULKAN_TEXTURE_PIPELINE (op->render.pipeline),
                                                             data + n_bytes + offset,
                                                             &op->render.offset,
                                                             &op->render.node->bounds,
                                                             &op->render.source_rect);
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_TEXT:
          {
            op->text.vertex_offset = offset + n_bytes;
            gsk_vulkan_text_pipeline_collect_vertex_data (GSK_VULKAN_TEXT_PIPELINE (op->text.pipeline),
                                                          data + n_bytes + offset,
                                                          GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render)),
                                                          &op->text.node->bounds,
                                                          (PangoFont *)gsk_text_node_get_font (op->text.node),
                                                          gsk_text_node_get_num_glyphs (op->text.node),
                                                          gsk_text_node_get_glyphs (op->text.node, NULL),
                                                          gsk_text_node_get_color (op->text.node),
                                                          &GRAPHENE_POINT_INIT (
                                                            gsk_text_node_get_offset (op->text.node)->x + op->render.offset.x,
                                                            gsk_text_node_get_offset (op->text.node)->y + op->render.offset.y
                                                          ),
                                                          op->text.start_glyph,
                                                          op->text.num_glyphs,
                                                          op->text.scale);
            n_bytes += op->text.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_COLOR_TEXT:
          {
            op->text.vertex_offset = offset + n_bytes;
            gsk_vulkan_color_text_pipeline_collect_vertex_data (GSK_VULKAN_COLOR_TEXT_PIPELINE (op->text.pipeline),
                                                                data + n_bytes + offset,
                                                                GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render)),
                                                                &op->text.node->bounds,
                                                                (PangoFont *)gsk_text_node_get_font (op->text.node),
                                                                gsk_text_node_get_num_glyphs (op->text.node),
                                                                gsk_text_node_get_glyphs (op->text.node, NULL),
                                                                &GRAPHENE_POINT_INIT (
                                                                  gsk_text_node_get_offset (op->text.node)->x + op->render.offset.x,
                                                                  gsk_text_node_get_offset (op->text.node)->y + op->render.offset.y
                                                                ),
                                                                op->text.start_glyph,
                                                                op->text.num_glyphs,
                                                                op->text.scale);
            n_bytes += op->text.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_COLOR:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_color_pipeline_collect_vertex_data (GSK_VULKAN_COLOR_PIPELINE (op->render.pipeline),
                                                           data + n_bytes + offset,
                                                           &op->render.offset,
                                                           &op->render.node->bounds,
                                                           gsk_color_node_get_color (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_LINEAR_GRADIENT:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_linear_gradient_pipeline_collect_vertex_data (GSK_VULKAN_LINEAR_GRADIENT_PIPELINE (op->render.pipeline),
                                                                     data + n_bytes + offset,
                                                                     &op->render.offset,
                                                                     &op->render.node->bounds,
                                                                     gsk_linear_gradient_node_get_start (op->render.node),
                                                                     gsk_linear_gradient_node_get_end (op->render.node),
                                                                     gsk_render_node_get_node_type (op->render.node) == GSK_REPEATING_LINEAR_GRADIENT_NODE,
                                                                     gsk_linear_gradient_node_get_n_color_stops (op->render.node),
                                                                     gsk_linear_gradient_node_get_color_stops (op->render.node, NULL));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_OPACITY:
          {
            graphene_matrix_t color_matrix;
            graphene_vec4_t color_offset;

            graphene_matrix_init_from_float (&color_matrix,
                                             (float[16]) {
                                                 1.0, 0.0, 0.0, 0.0,
                                                 0.0, 1.0, 0.0, 0.0,
                                                 0.0, 0.0, 1.0, 0.0,
                                                 0.0, 0.0, 0.0, gsk_opacity_node_get_opacity (op->render.node)
                                             });
            graphene_vec4_init (&color_offset, 0.0, 0.0, 0.0, 0.0);
            op->render.vertex_offset = offset + n_bytes;

            gsk_vulkan_effect_pipeline_collect_vertex_data (GSK_VULKAN_EFFECT_PIPELINE (op->render.pipeline),
                                                            data + n_bytes + offset,
                                                            &op->render.offset,
                                                            &op->render.node->bounds,
                                                            &op->render.source_rect,
                                                            &color_matrix,
                                                            &color_offset);
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_BLUR:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_blur_pipeline_collect_vertex_data (GSK_VULKAN_BLUR_PIPELINE (op->render.pipeline),
                                                          data + n_bytes + offset,
                                                          &op->render.offset,
                                                          &op->render.node->bounds,
                                                          &op->render.source_rect,
                                                          gsk_blur_node_get_radius (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_COLOR_MATRIX:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_effect_pipeline_collect_vertex_data (GSK_VULKAN_EFFECT_PIPELINE (op->render.pipeline),
                                                            data + n_bytes + offset,
                                                            &op->render.offset,
                                                            &op->render.node->bounds,
                                                            &op->render.source_rect,
                                                            gsk_color_matrix_node_get_color_matrix (op->render.node),
                                                            gsk_color_matrix_node_get_color_offset (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_BORDER:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_border_pipeline_collect_vertex_data (GSK_VULKAN_BORDER_PIPELINE (op->render.pipeline),
                                                            data + n_bytes + offset,
                                                            &op->render.offset,
                                                            gsk_border_node_get_outline (op->render.node),
                                                            gsk_border_node_get_widths (op->render.node),
                                                            gsk_border_node_get_colors (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_INSET_SHADOW:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_box_shadow_pipeline_collect_vertex_data (GSK_VULKAN_BOX_SHADOW_PIPELINE (op->render.pipeline),
                                                                data + n_bytes + offset,
                                                                &op->render.offset,
                                                                gsk_inset_shadow_node_get_outline (op->render.node),
                                                                gsk_inset_shadow_node_get_color (op->render.node),
                                                                gsk_inset_shadow_node_get_dx (op->render.node),
                                                                gsk_inset_shadow_node_get_dy (op->render.node),
                                                                gsk_inset_shadow_node_get_spread (op->render.node),
                                                                gsk_inset_shadow_node_get_blur_radius (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_OUTSET_SHADOW:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_box_shadow_pipeline_collect_vertex_data (GSK_VULKAN_BOX_SHADOW_PIPELINE (op->render.pipeline),
                                                                data + n_bytes + offset,
                                                                &op->render.offset,
                                                                gsk_outset_shadow_node_get_outline (op->render.node),
                                                                gsk_outset_shadow_node_get_color (op->render.node),
                                                                gsk_outset_shadow_node_get_dx (op->render.node),
                                                                gsk_outset_shadow_node_get_dy (op->render.node),
                                                                gsk_outset_shadow_node_get_spread (op->render.node),
                                                                gsk_outset_shadow_node_get_blur_radius (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_CROSS_FADE:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_cross_fade_pipeline_collect_vertex_data (GSK_VULKAN_CROSS_FADE_PIPELINE (op->render.pipeline),
                                                                data + n_bytes + offset,
                                                                &op->render.offset,
                                                                &op->render.node->bounds,
                                                                &op->render.source_rect,
                                                                &op->render.source2_rect,
                                                                gsk_cross_fade_node_get_progress (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_BLEND_MODE:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_blend_mode_pipeline_collect_vertex_data (GSK_VULKAN_BLEND_MODE_PIPELINE (op->render.pipeline),
                                                                data + n_bytes + offset,
                                                                &op->render.offset,
                                                                &op->render.node->bounds,
                                                                &op->render.source_rect,
                                                                &op->render.source2_rect,
                                                                gsk_blend_node_get_blend_mode (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
          continue;
        }

      g_assert (n_bytes + offset <= total);
    }

  return n_bytes;
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
      gsk_vulkan_render_pass_collect_vertex_data (self, render, data, 0, n_bytes);
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

  for (i = 0; i < self->render_ops->len; i++)
    {
      op = &g_array_index (self->render_ops, GskVulkanOp, i);

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
        case GSK_VULKAN_OP_FALLBACK_CLIP:
        case GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP:
        case GSK_VULKAN_OP_TEXTURE:
        case GSK_VULKAN_OP_OPACITY:
        case GSK_VULKAN_OP_BLUR:
        case GSK_VULKAN_OP_COLOR_MATRIX:
          if (op->render.source)
            op->render.descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->render.source, FALSE);
          break;

        case GSK_VULKAN_OP_REPEAT:
          if (op->render.source)
            op->render.descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->render.source, TRUE);
          break;

        case GSK_VULKAN_OP_TEXT:
        case GSK_VULKAN_OP_COLOR_TEXT:
          op->text.descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->text.source, FALSE);
          break;

        case GSK_VULKAN_OP_CROSS_FADE:
        case GSK_VULKAN_OP_BLEND_MODE:
          if (op->render.source && op->render.source2)
            {
              op->render.descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->render.source, FALSE);
              op->render.descriptor_set_index2 = gsk_vulkan_render_reserve_descriptor_set (render, op->render.source2, FALSE);
            }
          break;

        default:
          g_assert_not_reached ();

        case GSK_VULKAN_OP_COLOR:
        case GSK_VULKAN_OP_LINEAR_GRADIENT:
        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
        case GSK_VULKAN_OP_BORDER:
        case GSK_VULKAN_OP_INSET_SHADOW:
        case GSK_VULKAN_OP_OUTSET_SHADOW:
          break;
        }
    }
}

static void
gsk_vulkan_render_pass_draw_rect (GskVulkanRenderPass     *self,
                                  GskVulkanRender         *render,
                                  guint                    layout_count,
                                  VkPipelineLayout        *pipeline_layout,
                                  VkCommandBuffer          command_buffer)
{
  GskVulkanPipeline *current_pipeline = NULL;
  gsize current_draw_index = 0;
  GskVulkanOp *op;
  guint i, step;
  GskVulkanBuffer *vertex_buffer;

  vertex_buffer = gsk_vulkan_render_pass_get_vertex_data (self, render);

  for (i = 0; i < self->render_ops->len; i += step)
    {
      op = &g_array_index (self->render_ops, GskVulkanOp, i);
      step = 1;

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
        case GSK_VULKAN_OP_FALLBACK_CLIP:
        case GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP:
        case GSK_VULKAN_OP_TEXTURE:
        case GSK_VULKAN_OP_REPEAT:
          if (!op->render.source)
            continue;
          if (current_pipeline != op->render.pipeline)
            {
              current_pipeline = op->render.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->render.vertex_offset });
              current_draw_index = 0;
            }

          vkCmdBindDescriptorSets (command_buffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   gsk_vulkan_pipeline_get_pipeline_layout (current_pipeline),
                                   0,
                                   1,
                                   (VkDescriptorSet[1]) {
                                       gsk_vulkan_render_get_descriptor_set (render, op->render.descriptor_set_index)
                                   },
                                   0,
                                   NULL);

          current_draw_index += gsk_vulkan_texture_pipeline_draw (GSK_VULKAN_TEXTURE_PIPELINE (current_pipeline),
                                                                  command_buffer,
                                                                  current_draw_index, 1);
          break;

        case GSK_VULKAN_OP_TEXT:
          if (current_pipeline != op->text.pipeline)
            {
              current_pipeline = op->text.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->text.vertex_offset });
              current_draw_index = 0;
            }

          vkCmdBindDescriptorSets (command_buffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   gsk_vulkan_pipeline_get_pipeline_layout (current_pipeline),
                                   0,
                                   1,
                                   (VkDescriptorSet[1]) {
                                       gsk_vulkan_render_get_descriptor_set (render, op->text.descriptor_set_index)
                                   },
                                   0,
                                   NULL);

          current_draw_index += gsk_vulkan_text_pipeline_draw (GSK_VULKAN_TEXT_PIPELINE (current_pipeline),
                                                               command_buffer,
                                                               current_draw_index, op->text.num_glyphs);
          break;

        case GSK_VULKAN_OP_COLOR_TEXT:
          if (current_pipeline != op->text.pipeline)
            {
              current_pipeline = op->text.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->text.vertex_offset });
              current_draw_index = 0;
            }

          vkCmdBindDescriptorSets (command_buffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   gsk_vulkan_pipeline_get_pipeline_layout (current_pipeline),
                                   0,
                                   1,
                                   (VkDescriptorSet[1]) {
                                       gsk_vulkan_render_get_descriptor_set (render, op->text.descriptor_set_index)
                                   },
                                   0,
                                   NULL);

          current_draw_index += gsk_vulkan_color_text_pipeline_draw (GSK_VULKAN_COLOR_TEXT_PIPELINE (current_pipeline),
                                                                     command_buffer,
                                                                     current_draw_index, op->text.num_glyphs);
          break;

        case GSK_VULKAN_OP_OPACITY:
        case GSK_VULKAN_OP_COLOR_MATRIX:
          if (!op->render.source)
            continue;
          if (current_pipeline != op->render.pipeline)
            {
              current_pipeline = op->render.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->render.vertex_offset });
              current_draw_index = 0;
            }

          vkCmdBindDescriptorSets (command_buffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   gsk_vulkan_pipeline_get_pipeline_layout (current_pipeline),
                                   0,
                                   1,
                                   (VkDescriptorSet[1]) {
                                       gsk_vulkan_render_get_descriptor_set (render, op->render.descriptor_set_index)
                                   },
                                   0,
                                   NULL);

          current_draw_index += gsk_vulkan_effect_pipeline_draw (GSK_VULKAN_EFFECT_PIPELINE (current_pipeline),
                                                                 command_buffer,
                                                                 current_draw_index, 1);
          break;

        case GSK_VULKAN_OP_BLUR:
          if (!op->render.source)
            continue;
          if (current_pipeline != op->render.pipeline)
            {
              current_pipeline = op->render.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->render.vertex_offset });
              current_draw_index = 0;
            }

          vkCmdBindDescriptorSets (command_buffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   gsk_vulkan_pipeline_get_pipeline_layout (current_pipeline),
                                   0,
                                   1,
                                   (VkDescriptorSet[1]) {
                                       gsk_vulkan_render_get_descriptor_set (render, op->render.descriptor_set_index)
                                   },
                                   0,
                                   NULL);

          current_draw_index += gsk_vulkan_blur_pipeline_draw (GSK_VULKAN_BLUR_PIPELINE (current_pipeline),
                                                               command_buffer,
                                                               current_draw_index, 1);
          break;

        case GSK_VULKAN_OP_COLOR:
          if (current_pipeline != op->render.pipeline)
            {
              current_pipeline = op->render.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->render.vertex_offset });
              current_draw_index = 0;
            }

          for (step = 1; step + i < self->render_ops->len; step++)
            {
              GskVulkanOp *cmp = &g_array_index (self->render_ops, GskVulkanOp, i + step);
              if (cmp->type != GSK_VULKAN_OP_COLOR || 
                  cmp->render.pipeline != current_pipeline)
                break;
            }
          current_draw_index += gsk_vulkan_color_pipeline_draw (GSK_VULKAN_COLOR_PIPELINE (current_pipeline),
                                                                command_buffer,
                                                                current_draw_index, step);
          break;

        case GSK_VULKAN_OP_LINEAR_GRADIENT:
          if (current_pipeline != op->render.pipeline)
            {
              current_pipeline = op->render.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->render.vertex_offset });
              current_draw_index = 0;
            }
          current_draw_index += gsk_vulkan_linear_gradient_pipeline_draw (GSK_VULKAN_LINEAR_GRADIENT_PIPELINE (current_pipeline),
                                                                          command_buffer,
                                                                          current_draw_index, 1);
          break;

        case GSK_VULKAN_OP_BORDER:
          if (current_pipeline != op->render.pipeline)
            {
              current_pipeline = op->render.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->render.vertex_offset });
              current_draw_index = 0;
            }
          current_draw_index += gsk_vulkan_border_pipeline_draw (GSK_VULKAN_BORDER_PIPELINE (current_pipeline),
                                                                 command_buffer,
                                                                 current_draw_index, 1);
          break;

        case GSK_VULKAN_OP_INSET_SHADOW:
        case GSK_VULKAN_OP_OUTSET_SHADOW:
          if (current_pipeline != op->render.pipeline)
            {
              current_pipeline = op->render.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->render.vertex_offset });
              current_draw_index = 0;
            }
          current_draw_index += gsk_vulkan_box_shadow_pipeline_draw (GSK_VULKAN_BOX_SHADOW_PIPELINE (current_pipeline),
                                                                     command_buffer,
                                                                     current_draw_index, 1);
          break;

        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
          for (int j = 0; j < layout_count; j++)
            gsk_vulkan_push_constants_push (command_buffer,
                                            pipeline_layout[j],
                                            &op->constants.constants.mvp,
                                            &op->constants.constants.clip.rect);

          break;

        case GSK_VULKAN_OP_CROSS_FADE:
          if (!op->render.source || !op->render.source2)
            continue;
          if (current_pipeline != op->render.pipeline)
            {
              current_pipeline = op->render.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->render.vertex_offset });
              current_draw_index = 0;
            }

          vkCmdBindDescriptorSets (command_buffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   gsk_vulkan_pipeline_get_pipeline_layout (current_pipeline),
                                   0,
                                   2,
                                   (VkDescriptorSet[2]) {
                                       gsk_vulkan_render_get_descriptor_set (render, op->render.descriptor_set_index),
                                       gsk_vulkan_render_get_descriptor_set (render, op->render.descriptor_set_index2)
                                   },
                                   0,
                                   NULL);

          current_draw_index += gsk_vulkan_cross_fade_pipeline_draw (GSK_VULKAN_CROSS_FADE_PIPELINE (current_pipeline),
                                                                     command_buffer,
                                                                     current_draw_index, 1);
          break;

        case GSK_VULKAN_OP_BLEND_MODE:
          if (!op->render.source || !op->render.source2)
            continue;
          if (current_pipeline != op->render.pipeline)
            {
              current_pipeline = op->render.pipeline;
              vkCmdBindPipeline (command_buffer,
                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 gsk_vulkan_pipeline_get_pipeline (current_pipeline));
              vkCmdBindVertexBuffers (command_buffer,
                                      0,
                                      1,
                                      (VkBuffer[1]) {
                                          gsk_vulkan_buffer_get_buffer (vertex_buffer)
                                      },
                                      (VkDeviceSize[1]) { op->render.vertex_offset });
              current_draw_index = 0;
            }

          vkCmdBindDescriptorSets (command_buffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   gsk_vulkan_pipeline_get_pipeline_layout (current_pipeline),
                                   0,
                                   2,
                                   (VkDescriptorSet[2]) {
                                       gsk_vulkan_render_get_descriptor_set (render, op->render.descriptor_set_index),
                                       gsk_vulkan_render_get_descriptor_set (render, op->render.descriptor_set_index2)
                                   },
                                   0,
                                   NULL);

          current_draw_index += gsk_vulkan_blend_mode_pipeline_draw (GSK_VULKAN_BLEND_MODE_PIPELINE (current_pipeline),
                                                                     command_buffer,
                                                                     current_draw_index, 1);
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }
}

void
gsk_vulkan_render_pass_draw (GskVulkanRenderPass     *self,
                             GskVulkanRender         *render,
                             guint                    layout_count,
                             VkPipelineLayout        *pipeline_layout,
                             VkCommandBuffer          command_buffer)
{
  guint i;

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

  for (i = 0; i < cairo_region_num_rectangles (self->clip); i++)
    {
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (self->clip, i, &rect);

      vkCmdSetScissor (command_buffer,
                       0,
                       1,
                       &(VkRect2D) {
                          { rect.x, rect.y },
                          { rect.width, rect.height }
                       });

      vkCmdBeginRenderPass (command_buffer,
                            &(VkRenderPassBeginInfo) {
                                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                .renderPass = self->render_pass,
                                .framebuffer = gsk_vulkan_render_get_framebuffer (render, self->target),
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

      gsk_vulkan_render_pass_draw_rect (self, render, layout_count, pipeline_layout, command_buffer);

      vkCmdEndRenderPass (command_buffer);
    }
}

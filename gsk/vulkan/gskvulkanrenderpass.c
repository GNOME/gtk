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

#include <cairo-ft.h>

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
  int scale_factor;
  graphene_rect_t viewport;
  cairo_region_t *clip;
  graphene_matrix_t mv;
  graphene_matrix_t p;

  VkRenderPass render_pass;
  VkSemaphore signal_semaphore;
  GArray *wait_semaphores;
  GskVulkanBuffer *vertex_data;

  GQuark fallback_pixels;
  GQuark texture_pixels;
};

GskVulkanRenderPass *
gsk_vulkan_render_pass_new (GdkVulkanContext  *context,
                            GskVulkanImage    *target,
                            int                scale_factor,
                            graphene_matrix_t *mv,
                            graphene_rect_t   *viewport,
                            cairo_region_t    *clip,
                            VkSemaphore        signal_semaphore)
{
  GskVulkanRenderPass *self;
  VkImageLayout final_layout;

  self = g_slice_new0 (GskVulkanRenderPass);
  self->vulkan = g_object_ref (context);
  self->render_ops = g_array_new (FALSE, FALSE, sizeof (GskVulkanOp));

  self->target = g_object_ref (target);
  self->scale_factor = scale_factor;
  self->clip = cairo_region_copy (clip);
  self->viewport = *viewport;

  self->mv = *mv;
  graphene_matrix_init_ortho (&self->p,
                              viewport->origin.x, viewport->origin.x + viewport->size.width,
                              viewport->origin.y, viewport->origin.y + viewport->size.height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);

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
  self->fallback_pixels = g_quark_from_static_string ("fallback-pixels");
  self->texture_pixels = g_quark_from_static_string ("texture-pixels");
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


  g_slice_free (GskVulkanRenderPass, self);
}

static gboolean
font_has_color_glyphs (const PangoFont *font)
{
  cairo_scaled_font_t *scaled_font;
  gboolean has_color = FALSE;

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)font);
  if (cairo_scaled_font_get_type (scaled_font) == CAIRO_FONT_TYPE_FT)
    {
      FT_Face ft_face = cairo_ft_scaled_font_lock_face (scaled_font);
      has_color = (FT_HAS_COLOR (ft_face) != 0);
      cairo_ft_scaled_font_unlock_face (scaled_font);
    }

  return has_color;
}

#define FALLBACK(...) G_STMT_START { \
  GSK_RENDERER_NOTE (gsk_vulkan_render_get_renderer (render), FALLBACK, g_message (__VA_ARGS__)); \
  goto fallback; \
}G_STMT_END

static void
gsk_vulkan_render_pass_add_node (GskVulkanRenderPass           *self,
                                 GskVulkanRender               *render,
                                 const GskVulkanPushConstants  *constants,
                                 GskRenderNode                 *node)
{
  GskVulkanOp op = {
    .type = GSK_VULKAN_OP_FALLBACK,
    .render.node = node
  };
  GskVulkanPipelineType pipeline_type;

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      return;
    case GSK_SHADOW_NODE:
    default:
      FALLBACK ("Unsupported node '%s'", node->node_class->type_name);

    case GSK_REPEAT_NODE:
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
      return;

    case GSK_BLEND_NODE:
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
       return;

    case GSK_CROSS_FADE_NODE:
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
      return;

    case GSK_INSET_SHADOW_NODE:
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
      return;

    case GSK_OUTSET_SHADOW_NODE:
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
      return;

    case GSK_CAIRO_NODE:
      if (gsk_cairo_node_peek_surface (node) == NULL)
        return;
      /* We're using recording surfaces, so drawing them to an image
       * surface and uploading them is the right thing.
       * But that's exactly what the fallback code does.
       */
      goto fallback;

    case GSK_TEXT_NODE:
      {
        const PangoFont *font = gsk_text_node_peek_font (node);
        const PangoGlyphInfo *glyphs = gsk_text_node_peek_glyphs (node);
        guint num_glyphs = gsk_text_node_get_num_glyphs (node);
        int i;
        guint count;
        guint texture_index;
        gint x_position;
        GskVulkanRenderer *renderer = GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render));

        if (font_has_color_glyphs (font))
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
        op.text.scale = self->scale_factor;

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

        return;
      }

    case GSK_TEXTURE_NODE:
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
      return;

    case GSK_COLOR_NODE:
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
      return;

    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
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
      return;

    case GSK_OPACITY_NODE:
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
      return;

    case GSK_BLUR_NODE:
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
      return;

    case GSK_COLOR_MATRIX_NODE:
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
      return;

    case GSK_BORDER_NODE:
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
      return;

    case GSK_CONTAINER_NODE:
      {
        guint i;

        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          {
            gsk_vulkan_render_pass_add_node (self, render, constants, gsk_container_node_get_child (node, i));
          }
      }
      return;

    case GSK_DEBUG_NODE:
      gsk_vulkan_render_pass_add_node (self, render, constants, gsk_debug_node_get_child (node));
      return;

    case GSK_TRANSFORM_NODE:
      {
        graphene_matrix_t transform, mv;
        GskRenderNode *child;

#if 0
       if (!gsk_vulkan_clip_contains_rect (clip, &node->bounds))
          FALLBACK ("Transform nodes can't deal with clip type %u\n", clip->type);
#endif

        child = gsk_transform_node_get_child (node);
        gsk_transform_to_matrix (gsk_transform_node_get_transform (node), &transform);
        graphene_matrix_init_from_matrix (&mv, &self->mv);
        graphene_matrix_multiply (&transform, &mv, &self->mv);
        if (!gsk_vulkan_push_constants_transform (&op.constants.constants, constants, &transform, &child->bounds))
          FALLBACK ("Transform nodes can't deal with clip type %u", constants->clip.type);
        op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
        g_array_append_val (self->render_ops, op);

        gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, child);
        gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
        graphene_matrix_init_from_matrix (&self->mv, &mv);
        g_array_append_val (self->render_ops, op);
      }
      return;

    case GSK_CLIP_NODE:
      {
        if (!gsk_vulkan_push_constants_intersect_rect (&op.constants.constants, constants, gsk_clip_node_peek_clip (node)))
          FALLBACK ("Failed to find intersection between clip of type %u and rectangle", constants->clip.type);
        if (op.constants.constants.clip.type == GSK_VULKAN_CLIP_ALL_CLIPPED)
          return;

        op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
        g_array_append_val (self->render_ops, op);

        gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, gsk_clip_node_get_child (node));

        gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
        g_array_append_val (self->render_ops, op);
      }
      return;

    case GSK_ROUNDED_CLIP_NODE:
      {
        if (!gsk_vulkan_push_constants_intersect_rounded (&op.constants.constants,
                                                          constants,
                                                          gsk_rounded_clip_node_peek_clip (node)))
          FALLBACK ("Failed to find intersection between clip of type %u and rounded rectangle", constants->clip.type);
        if (op.constants.constants.clip.type == GSK_VULKAN_CLIP_ALL_CLIPPED)
          return;

        op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
        g_array_append_val (self->render_ops, op);

        gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, gsk_rounded_clip_node_get_child (node));

        gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
        g_array_append_val (self->render_ops, op);
      }
      return;
    }

  g_assert_not_reached ();
  return;

fallback:
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
        return;
    }
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_TEXTURE);
  g_array_append_val (self->render_ops, op);
}
#undef FALLBACK

void
gsk_vulkan_render_pass_add (GskVulkanRenderPass     *self,
                            GskVulkanRender         *render,
                            GskRenderNode           *node)
{
  GskVulkanOp op = { 0, };
  graphene_matrix_t mvp;

  graphene_matrix_multiply (&self->mv, &self->p, &mvp);
  op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
  gsk_vulkan_push_constants_init (&op.constants.constants, &mvp, &self->viewport);
  g_array_append_val (self->render_ops, op);

  gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, node);
}

static GskVulkanImage *
gsk_vulkan_render_pass_get_node_as_texture (GskVulkanRenderPass   *self,
                                            GskVulkanRender       *render,
                                            GskVulkanUploader     *uploader,
                                            GskRenderNode         *node,
                                            const graphene_rect_t *bounds,
                                            GskVulkanClip         *current_clip,
                                            graphene_rect_t       *tex_rect)
{
  GskVulkanImage *result;
  cairo_surface_t *surface;
  cairo_t *cr;

  switch ((guint) gsk_render_node_get_node_type (node))
    {
    case GSK_TEXTURE_NODE:
      if (graphene_rect_equal (bounds, &node->bounds))
        {
          result = gsk_vulkan_renderer_ref_texture_image (GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render)),
                                                          gsk_texture_node_get_texture (node),
                                                          uploader);
          gsk_vulkan_render_add_cleanup_image (render, result);
          *tex_rect = GRAPHENE_RECT_INIT(0, 0, 1, 1);
          return result;
        }
      break;

    case GSK_CAIRO_NODE:
      /* We're using recording surfaces, so drawing them to an image
       * surface and uploading them is the right thing.
       * But that's exactly what the fallback code does.
       */
      break;

    default:
      {
        VkSemaphore semaphore;
        graphene_rect_t view;
        cairo_region_t *clip;
        GskVulkanRenderPass *pass;
        graphene_rect_t clipped;

        if (current_clip)
          graphene_rect_intersection (&current_clip->rect.bounds, bounds, &clipped);
        else
          clipped = *bounds;

        if (clipped.size.width == 0 || clipped.size.height == 0)
          return NULL;

        graphene_matrix_transform_bounds (&self->mv, &clipped, &view);
        view.origin.x = floor (view.origin.x);
        view.origin.y = floor (view.origin.y);
        view.size.width = ceil (view.size.width);
        view.size.height = ceil (view.size.height);

        result = gsk_vulkan_image_new_for_texture (self->vulkan,
                                                   view.size.width,
                                                   view.size.height);

#ifdef G_ENABLE_DEBUG
        {
          GskProfiler *profiler = gsk_renderer_get_profiler (gsk_vulkan_render_get_renderer (render));
          gsk_profiler_counter_add (profiler,
                                    self->texture_pixels,
                                    view.size.width * view.size.height);
        }
#endif

        vkCreateSemaphore (gdk_vulkan_context_get_device (self->vulkan),
                           &(VkSemaphoreCreateInfo) {
                             VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                             NULL,
                             0
                           },
                           NULL,
                           &semaphore);

        g_array_append_val (self->wait_semaphores, semaphore);

        clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                                0, 0,
                                                gsk_vulkan_image_get_width (result),
                                                gsk_vulkan_image_get_height (result)
                                              });

        pass = gsk_vulkan_render_pass_new (self->vulkan,
                                           result,
                                           self->scale_factor,
                                           &self->mv,
                                           &view,
                                           clip,
                                           semaphore);

        cairo_region_destroy (clip);

        gsk_vulkan_render_add_render_pass (render, pass);
        gsk_vulkan_render_pass_add (pass, render, node);
        gsk_vulkan_render_add_cleanup_image (render, result);

        /* assuming the unclipped bounds should go to texture coordinates 0..1,
         * calculate the coordinates for the clipped texture size
         */
        tex_rect->origin.x = (bounds->origin.x - clipped.origin.x)/clipped.size.width;
        tex_rect->origin.y = (bounds->origin.y - clipped.origin.y)/clipped.size.height;
        tex_rect->size.width = bounds->size.width/clipped.size.width;
        tex_rect->size.height = bounds->size.height/clipped.size.height;

        return result;
      }
   }

  GSK_RENDERER_NOTE (gsk_vulkan_render_get_renderer (render), FALLBACK, g_message ("Node as texture not implemented for this case. Using %gx%g fallback surface",
                               ceil (bounds->size.width),
                               ceil (bounds->size.height)));
#ifdef G_ENABLE_DEBUG
  {
    GskProfiler *profiler = gsk_renderer_get_profiler (gsk_vulkan_render_get_renderer (render));
    gsk_profiler_counter_add (profiler,
                              self->fallback_pixels,
                              ceil (bounds->size.width) * ceil (bounds->size.height));
  }
#endif

  /* XXX: We could intersect bounds with clip bounds here */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        ceil (bounds->size.width),
                                        ceil (bounds->size.height));
  cr = cairo_create (surface);
  cairo_translate (cr, -bounds->origin.x, -bounds->origin.y);

  gsk_render_node_draw (node, cr);

  cairo_destroy (cr);

  result = gsk_vulkan_image_new_from_data (uploader,
                                           cairo_image_surface_get_data (surface),
                                           cairo_image_surface_get_width (surface),
                                           cairo_image_surface_get_height (surface),
                                           cairo_image_surface_get_stride (surface));

  cairo_surface_destroy (surface);

  gsk_vulkan_render_add_cleanup_image (render, result);

  tex_rect->origin.x = (node->bounds.origin.x - bounds->origin.x)/bounds->size.width;
  tex_rect->origin.y = (node->bounds.origin.y - bounds->origin.y)/bounds->size.height;
  tex_rect->size.width = node->bounds.size.width/bounds->size.width;
  tex_rect->size.height = node->bounds.size.height/bounds->size.height;

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

  node = op->node;

  GSK_RENDERER_NOTE (gsk_vulkan_render_get_renderer (render), FALLBACK,
            g_message ("Upload op=%s, node %s[%p], bounds %gx%g",
                     op->type == GSK_VULKAN_OP_FALLBACK_CLIP ? "fallback-clip" :
                     (op->type == GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP ? "fallback-rounded-clip" : "fallback"),
                     node->node_class->type_name, node,
                     ceil (node->bounds.size.width),
                     ceil (node->bounds.size.height)));
#ifdef G_ENABLE_DEBUG
  {
    GskProfiler *profiler = gsk_renderer_get_profiler (gsk_vulkan_render_get_renderer (render));
    gsk_profiler_counter_add (profiler,
                              self->fallback_pixels,
                              ceil (node->bounds.size.width) * ceil (node->bounds.size.height));
  }
#endif

  /* XXX: We could intersect bounds with clip bounds here */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        ceil (node->bounds.size.width * self->scale_factor),
                                        ceil (node->bounds.size.height * self->scale_factor));
  cairo_surface_set_device_scale (surface, self->scale_factor, self->scale_factor);
  cr = cairo_create (surface);
  cairo_translate (cr, -node->bounds.origin.x, -node->bounds.origin.y);

  if (op->type == GSK_VULKAN_OP_FALLBACK_CLIP)
    {
      cairo_rectangle (cr,
                       op->clip.bounds.origin.x, op->clip.bounds.origin.y,
                       op->clip.bounds.size.width, op->clip.bounds.size.height);
      cairo_clip (cr);
    }
  else if (op->type == GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP)
    {
      gsk_rounded_rect_path (&op->clip, cr);
      cairo_clip (cr);
    }
  else
    {
      g_assert (op->type == GSK_VULKAN_OP_FALLBACK);
    }

  gsk_render_node_draw (node, cr);

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

void
gsk_vulkan_render_pass_upload (GskVulkanRenderPass  *self,
                               GskVulkanRender      *render,
                               GskVulkanUploader    *uploader)
{
  GskVulkanOp *op;
  guint i;
  GskVulkanClip *clip = NULL;

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

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            child,
                                                                            &child->bounds,
                                                                            clip,
                                                                            &op->render.source_rect);
          }
          break;

        case GSK_VULKAN_OP_REPEAT:
          {
            GskRenderNode *child = gsk_repeat_node_get_child (op->render.node);
            const graphene_rect_t *bounds = &op->render.node->bounds;
            const graphene_rect_t *child_bounds = gsk_repeat_node_peek_child_bounds (op->render.node);

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            child,
                                                                            child_bounds,
                                                                            NULL,
                                                                            &op->render.source_rect);

            op->render.source_rect.origin.x = (bounds->origin.x - child_bounds->origin.x)/child_bounds->size.width;
            op->render.source_rect.origin.y = (bounds->origin.y - child_bounds->origin.y)/child_bounds->size.height;
            op->render.source_rect.size.width = bounds->size.width / child_bounds->size.width;
            op->render.source_rect.size.height = bounds->size.height / child_bounds->size.height;
          }
          break;

        case GSK_VULKAN_OP_BLUR:
          {
            GskRenderNode *child = gsk_blur_node_get_child (op->render.node);

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            child,
                                                                            &child->bounds,
                                                                            clip,
                                                                            &op->render.source_rect);
          }
          break;

        case GSK_VULKAN_OP_COLOR_MATRIX:
          {
            GskRenderNode *child = gsk_color_matrix_node_get_child (op->render.node);

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            child,
                                                                            &child->bounds,
                                                                            clip,
                                                                            &op->render.source_rect);
          }
          break;

        case GSK_VULKAN_OP_CROSS_FADE:
          {
            GskRenderNode *start = gsk_cross_fade_node_get_start_child (op->render.node);
            GskRenderNode *end = gsk_cross_fade_node_get_end_child (op->render.node);
            const graphene_rect_t *bounds = &op->render.node->bounds;

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            start,
                                                                            &start->bounds,
                                                                            clip,
                                                                            &op->render.source_rect);
            op->render.source_rect.origin.x = (bounds->origin.x - start->bounds.origin.x)/start->bounds.size.width;
            op->render.source_rect.origin.y = (bounds->origin.y - start->bounds.origin.y)/start->bounds.size.height;
            op->render.source_rect.size.width = bounds->size.width / start->bounds.size.width;
            op->render.source_rect.size.height = bounds->size.height / start->bounds.size.height;

            op->render.source2 = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                             render,
                                                                             uploader,
                                                                             end,
                                                                             &end->bounds,
                                                                             clip,
                                                                             &op->render.source2_rect);
            op->render.source2_rect.origin.x = (bounds->origin.x - end->bounds.origin.x)/end->bounds.size.width;
            op->render.source2_rect.origin.y = (bounds->origin.y - end->bounds.origin.y)/end->bounds.size.height;
            op->render.source2_rect.size.width = bounds->size.width / end->bounds.size.width;
            op->render.source2_rect.size.height = bounds->size.height / end->bounds.size.height;
          }
          break;

        case GSK_VULKAN_OP_BLEND_MODE:
          {
            GskRenderNode *top = gsk_blend_node_get_top_child (op->render.node);
            GskRenderNode *bottom = gsk_blend_node_get_bottom_child (op->render.node);
            const graphene_rect_t *bounds = &op->render.node->bounds;

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            top,
                                                                            &top->bounds,
                                                                            clip,
                                                                            &op->render.source_rect);
            op->render.source_rect.origin.x = (bounds->origin.x - top->bounds.origin.x)/top->bounds.size.width;
            op->render.source_rect.origin.y = (bounds->origin.y - top->bounds.origin.y)/top->bounds.size.height;
            op->render.source_rect.size.width = bounds->size.width / top->bounds.size.width;
            op->render.source_rect.size.height = bounds->size.height / top->bounds.size.height;

            op->render.source2 = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                             render,
                                                                             uploader,
                                                                             bottom,
                                                                             &bottom->bounds,
                                                                             clip,
                                                                             &op->render.source2_rect);
            op->render.source2_rect.origin.x = (bounds->origin.x - bottom->bounds.origin.x)/bottom->bounds.size.width;
            op->render.source2_rect.origin.y = (bounds->origin.y - bottom->bounds.origin.y)/bottom->bounds.size.height;
            op->render.source2_rect.size.width = bounds->size.width / bottom->bounds.size.width;
            op->render.source2_rect.size.height = bounds->size.height / bottom->bounds.size.height;
          }
          break;

        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
          clip = &op->constants.constants.clip;
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
                                                          (PangoFont *)gsk_text_node_peek_font (op->text.node),
                                                          gsk_text_node_get_num_glyphs (op->text.node),
                                                          gsk_text_node_peek_glyphs (op->text.node),
                                                          gsk_text_node_peek_color (op->text.node),
                                                          gsk_text_node_get_offset (op->text.node),
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
                                                                (PangoFont *)gsk_text_node_peek_font (op->text.node),
                                                                gsk_text_node_get_num_glyphs (op->text.node),
                                                                gsk_text_node_peek_glyphs (op->text.node),
                                                                gsk_text_node_get_offset (op->text.node),
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
                                                           &op->render.node->bounds,
                                                           gsk_color_node_peek_color (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_LINEAR_GRADIENT:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_linear_gradient_pipeline_collect_vertex_data (GSK_VULKAN_LINEAR_GRADIENT_PIPELINE (op->render.pipeline),
                                                                     data + n_bytes + offset,
                                                                     &op->render.node->bounds,
                                                                     gsk_linear_gradient_node_peek_start (op->render.node),
                                                                     gsk_linear_gradient_node_peek_end (op->render.node),
                                                                     gsk_render_node_get_node_type (op->render.node) == GSK_REPEATING_LINEAR_GRADIENT_NODE,
                                                                     gsk_linear_gradient_node_get_n_color_stops (op->render.node),
                                                                     gsk_linear_gradient_node_peek_color_stops (op->render.node));
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
                                                            &op->render.node->bounds,
                                                            &op->render.source_rect,
                                                            gsk_color_matrix_node_peek_color_matrix (op->render.node),
                                                            gsk_color_matrix_node_peek_color_offset (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_BORDER:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_border_pipeline_collect_vertex_data (GSK_VULKAN_BORDER_PIPELINE (op->render.pipeline),
                                                            data + n_bytes + offset,
                                                            gsk_border_node_peek_outline (op->render.node),
                                                            gsk_border_node_peek_widths (op->render.node),
                                                            gsk_border_node_peek_colors (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_INSET_SHADOW:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_box_shadow_pipeline_collect_vertex_data (GSK_VULKAN_BOX_SHADOW_PIPELINE (op->render.pipeline),
                                                                data + n_bytes + offset,
                                                                gsk_inset_shadow_node_peek_outline (op->render.node),
                                                                gsk_inset_shadow_node_peek_color (op->render.node),
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
                                                                gsk_outset_shadow_node_peek_outline (op->render.node),
                                                                gsk_outset_shadow_node_peek_color (op->render.node),
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
            gsk_vulkan_push_constants_push (&op->constants.constants,
                                            command_buffer,
                                            pipeline_layout[j]);
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
                          { rect.x * self->scale_factor, rect.y * self->scale_factor },
                          { rect.width * self->scale_factor, rect.height * self->scale_factor }
                       });

      vkCmdBeginRenderPass (command_buffer,
                            &(VkRenderPassBeginInfo) {
                                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                .renderPass = self->render_pass,
                                .framebuffer = gsk_vulkan_render_get_framebuffer (render, self->target),
                                .renderArea = { 
                                    { rect.x * self->scale_factor, rect.y * self->scale_factor },
                                    { rect.width * self->scale_factor, rect.height * self->scale_factor }
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

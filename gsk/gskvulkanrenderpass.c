#include "config.h"

#include "gskvulkanrenderpassprivate.h"

#include "gskdebugprivate.h"
#include "gskprofilerprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderer.h"
#include "gskrendererprivate.h"
#include "gskroundedrectprivate.h"
#include "gskvulkanblendmodepipelineprivate.h"
#include "gskvulkanblendpipelineprivate.h"
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
#include "gskvulkanimageprivate.h"
#include "gskvulkanpushconstantsprivate.h"
#include "gskvulkanrendererprivate.h"
#include "gskprivate.h"

#include <cairo-ft.h>

typedef union _GskVulkanOp GskVulkanOp;
typedef struct _GskVulkanOpRender GskVulkanOpRender;
typedef struct _GskVulkanOpText GskVulkanOpText;
typedef struct _GskVulkanOpPushConstants GskVulkanOpPushConstants;

typedef enum {
  /* GskVulkanOpRender */
  GSK_VULKAN_OP_FALLBACK,
  GSK_VULKAN_OP_FALLBACK_CLIP,
  GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP,
  GSK_VULKAN_OP_SURFACE,
  GSK_VULKAN_OP_TEXTURE,
  GSK_VULKAN_OP_COLOR,
  GSK_VULKAN_OP_LINEAR_GRADIENT,
  GSK_VULKAN_OP_OPACITY,
  GSK_VULKAN_OP_BLUR,
  GSK_VULKAN_OP_COLOR_MATRIX,
  GSK_VULKAN_OP_BORDER,
  GSK_VULKAN_OP_INSET_SHADOW,
  GSK_VULKAN_OP_OUTSET_SHADOW,
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

  GQuark fallback_pixels;
};

GskVulkanRenderPass *
gsk_vulkan_render_pass_new (GdkVulkanContext *context)
{
  GskVulkanRenderPass *self;

  self = g_slice_new0 (GskVulkanRenderPass);
  self->vulkan = g_object_ref (context);
  self->render_ops = g_array_new (FALSE, FALSE, sizeof (GskVulkanOp));
#ifdef G_ENABLE_DEBUG
  self->fallback_pixels = g_quark_from_static_string ("fallback-pixels");
#endif

  return self;
}

void
gsk_vulkan_render_pass_free (GskVulkanRenderPass *self)
{
  g_array_unref (self->render_ops);
  g_object_unref (self->vulkan);

  g_slice_free (GskVulkanRenderPass, self);
}

static gboolean
font_has_color_glyphs (PangoFont *font)
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
  GSK_NOTE (FALLBACK, g_print (__VA_ARGS__)); \
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
    case GSK_REPEAT_NODE:
    case GSK_SHADOW_NODE:
    default:
      FALLBACK ("Unsupported node '%s'\n", node->node_class->type_name);

    case GSK_BLEND_NODE:
      if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
        pipeline_type = GSK_VULKAN_PIPELINE_BLEND_MODE;
      else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
        pipeline_type = GSK_VULKAN_PIPELINE_BLEND_MODE_CLIP;
      else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
        pipeline_type = GSK_VULKAN_PIPELINE_BLEND_MODE_CLIP_ROUNDED;
      else
        FALLBACK ("Blend nodes can't deal with clip type %u\n", constants->clip.type);
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
        FALLBACK ("Cross fade nodes can't deal with clip type %u\n", constants->clip.type);
      op.type = GSK_VULKAN_OP_CROSS_FADE;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
      g_array_append_val (self->render_ops, op);
      return;

    case GSK_INSET_SHADOW_NODE:
      if (gsk_inset_shadow_node_get_blur_radius (node) > 0)
        FALLBACK ("Blur support not implemented for inset shadows\n");
      else if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
        pipeline_type = GSK_VULKAN_PIPELINE_INSET_SHADOW;
      else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
        pipeline_type = GSK_VULKAN_PIPELINE_INSET_SHADOW_CLIP;
      else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
        pipeline_type = GSK_VULKAN_PIPELINE_INSET_SHADOW_CLIP_ROUNDED;
      else
        FALLBACK ("Inset shadow nodes can't deal with clip type %u\n", constants->clip.type);
      op.type = GSK_VULKAN_OP_INSET_SHADOW;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
      g_array_append_val (self->render_ops, op);
      return;

    case GSK_OUTSET_SHADOW_NODE:
      if (gsk_outset_shadow_node_get_blur_radius (node) > 0)
        FALLBACK ("Blur support not implemented for outset shadows\n");
      else if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
        pipeline_type = GSK_VULKAN_PIPELINE_OUTSET_SHADOW;
      else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
        pipeline_type = GSK_VULKAN_PIPELINE_OUTSET_SHADOW_CLIP;
      else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
        pipeline_type = GSK_VULKAN_PIPELINE_OUTSET_SHADOW_CLIP_ROUNDED;
      else
        FALLBACK ("Outset shadow nodes can't deal with clip type %u\n", constants->clip.type);
      op.type = GSK_VULKAN_OP_OUTSET_SHADOW;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
      g_array_append_val (self->render_ops, op);
      return;

    case GSK_CAIRO_NODE:
      if (gsk_cairo_node_get_surface (node) == NULL)
        return;
      if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
        pipeline_type = GSK_VULKAN_PIPELINE_BLEND;
      else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
        pipeline_type = GSK_VULKAN_PIPELINE_BLEND_CLIP;
      else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
        pipeline_type = GSK_VULKAN_PIPELINE_BLEND_CLIP_ROUNDED;
      else
        FALLBACK ("Cairo nodes can't deal with clip type %u\n", constants->clip.type);
      op.type = GSK_VULKAN_OP_SURFACE;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
      g_array_append_val (self->render_ops, op);
      return;

    case GSK_TEXT_NODE:
      {
        PangoFont *font = gsk_text_node_get_font (node);
        PangoGlyphString *glyphs = gsk_text_node_get_glyphs (node);
        int i;
        guint count;
        guint texture_index;
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
              FALLBACK ("Text nodes can't deal with clip type %u\n", constants->clip.type);
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
              FALLBACK ("Text nodes can't deal with clip type %u\n", constants->clip.type);
            op.type = GSK_VULKAN_OP_TEXT;
          }
        op.text.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);

        op.text.start_glyph = 0;
        op.text.texture_index = G_MAXUINT;

        for (i = 0, count = 0; i < glyphs->num_glyphs; i++)
          {
            PangoGlyphInfo *gi = &glyphs->glyphs[i];

            if (gi->glyph != PANGO_GLYPH_EMPTY && !(gi->glyph & PANGO_GLYPH_UNKNOWN_FLAG))
              {
                texture_index = gsk_vulkan_renderer_cache_glyph (renderer, font, gi->glyph);
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
              }
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
        pipeline_type = GSK_VULKAN_PIPELINE_BLEND;
      else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
        pipeline_type = GSK_VULKAN_PIPELINE_BLEND_CLIP;
      else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
        pipeline_type = GSK_VULKAN_PIPELINE_BLEND_CLIP_ROUNDED;
      else
        FALLBACK ("Texture nodes can't deal with clip type %u\n", constants->clip.type);
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
        FALLBACK ("Color nodes can't deal with clip type %u\n", constants->clip.type);
      op.type = GSK_VULKAN_OP_COLOR;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, pipeline_type);
      g_array_append_val (self->render_ops, op);
      return;

    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      if (gsk_linear_gradient_node_get_n_color_stops (node) > GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS)
        FALLBACK ("Linear gradient with %zu color stops, hardcoded limit is %u\n",
                  gsk_linear_gradient_node_get_n_color_stops (node),
                  GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS);
      if (gsk_vulkan_clip_contains_rect (&constants->clip, &node->bounds))
        pipeline_type = GSK_VULKAN_PIPELINE_LINEAR_GRADIENT;
      else if (constants->clip.type == GSK_VULKAN_CLIP_RECT)
        pipeline_type = GSK_VULKAN_PIPELINE_LINEAR_GRADIENT_CLIP;
      else if (constants->clip.type == GSK_VULKAN_CLIP_ROUNDED_CIRCULAR)
        pipeline_type = GSK_VULKAN_PIPELINE_LINEAR_GRADIENT_CLIP_ROUNDED;
      else
        FALLBACK ("Linear gradient nodes can't deal with clip type %u\n", constants->clip.type);
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
        FALLBACK ("Opacity nodes can't deal with clip type %u\n", constants->clip.type);
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
        FALLBACK ("Blur nodes can't deal with clip type %u\n", constants->clip.type);
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
        FALLBACK ("Color matrix nodes can't deal with clip type %u\n", constants->clip.type);
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
        FALLBACK ("Border nodes can't deal with clip type %u\n", constants->clip.type);
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

    case GSK_TRANSFORM_NODE:
      {
        graphene_matrix_t transform;
        GskRenderNode *child;

#if 0
        if (!gsk_vulkan_clip_contains_rect (clip, &node->bounds))
          FALLBACK ("Transform nodes can't deal with clip type %u\n", clip->type);
#endif

        gsk_transform_node_get_transform (node, &transform);
        child = gsk_transform_node_get_child (node);
        if (!gsk_vulkan_push_constants_transform (&op.constants.constants, constants, &transform, &child->bounds))
          FALLBACK ("Transform nodes can't deal with clip type %u\n", constants->clip.type);
        op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
        g_array_append_val (self->render_ops, op);

        gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, child);
        gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
        g_array_append_val (self->render_ops, op);
      }
      return;

    case GSK_CLIP_NODE:
      {
        if (!gsk_vulkan_push_constants_intersect_rect (&op.constants.constants, constants, gsk_clip_node_peek_clip (node)))
          FALLBACK ("Failed to find intersection between clip of type %u and rectangle\n", constants->clip.type);
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
          FALLBACK ("Failed to find intersection between clip of type %u and rounded rectangle\n", constants->clip.type);
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
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_BLEND);
  g_array_append_val (self->render_ops, op);
}
#undef FALLBACK

void
gsk_vulkan_render_pass_add (GskVulkanRenderPass     *self,
                            GskVulkanRender         *render,
                            const graphene_matrix_t *mvp,
                            const graphene_rect_t   *viewport,
                            GskRenderNode           *node)
{
  GskVulkanOp op = { 0, };

  op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
  gsk_vulkan_push_constants_init (&op.constants.constants, mvp, viewport);
  g_array_append_val (self->render_ops, op);

  gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, node);
}

static GskVulkanImage *
gsk_vulkan_render_pass_get_node_as_texture (GskVulkanRenderPass   *self,
                                            GskVulkanRender       *render,
                                            GskVulkanUploader     *uploader,
                                            GskRenderNode         *node,
                                            const graphene_rect_t *bounds)
{
  GskVulkanImage *result;
  cairo_surface_t *surface;
  cairo_t *cr;

  if (graphene_rect_equal (bounds, &node->bounds))
    {
      switch (gsk_render_node_get_node_type (node))
        {
        case GSK_TEXTURE_NODE:
          return gsk_vulkan_renderer_ref_texture_image (GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render)),
                                                        gsk_texture_node_get_texture (node),
                                                        uploader);
        case GSK_CAIRO_NODE:
          surface = cairo_surface_reference (gsk_cairo_node_get_surface (node));
          goto got_surface;

        default:
          break;
        }
    }

  GSK_NOTE (FALLBACK, g_print ("Node as texture not implemented. Using %gx%g fallback surface\n",
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

got_surface:
  result = gsk_vulkan_image_new_from_data (uploader,
                                           cairo_image_surface_get_data (surface),
                                           cairo_image_surface_get_width (surface),
                                           cairo_image_surface_get_height (surface),
                                           cairo_image_surface_get_stride (surface));

  cairo_surface_destroy (surface);

  gsk_vulkan_render_add_cleanup_image (render, result);

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

  GSK_NOTE (FALLBACK,
            g_print ("Upload op=%s, node %s[%p], bounds %gx%g\n",
                     op->type == GSK_VULKAN_OP_FALLBACK_CLIP ? "fallback-clip" :
                     (op->type == GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP ? "fallback-rounded-clip" : "fallback"),
                     node->name ? node->name : node->node_class->type_name, node,
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
                                        ceil (node->bounds.size.width),
                                        ceil (node->bounds.size.height));
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

        case GSK_VULKAN_OP_SURFACE:
          {
            cairo_surface_t *surface;

            surface = gsk_cairo_node_get_surface (op->render.node);
            op->render.source = gsk_vulkan_image_new_from_data (uploader,
                                                                cairo_image_surface_get_data (surface),
                                                                cairo_image_surface_get_width (surface),
                                                                cairo_image_surface_get_height (surface),
                                                                cairo_image_surface_get_stride (surface));
            gsk_vulkan_render_add_cleanup_image (render, op->render.source);
          }
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
                                                                            &child->bounds);
          }
          break;

        case GSK_VULKAN_OP_BLUR:
          {
            GskRenderNode *child = gsk_blur_node_get_child (op->render.node);

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            child,
                                                                            &child->bounds);
          }
          break;

        case GSK_VULKAN_OP_COLOR_MATRIX:
          {
            GskRenderNode *child = gsk_color_matrix_node_get_child (op->render.node);

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self, 
                                                                            render,
                                                                            uploader,
                                                                            child,
                                                                            &child->bounds);
          }
          break;

        case GSK_VULKAN_OP_CROSS_FADE:
          {
            GskRenderNode *start = gsk_cross_fade_node_get_start_child (op->render.node);
            GskRenderNode *end = gsk_cross_fade_node_get_end_child (op->render.node);

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            start,
                                                                            &start->bounds);
            op->render.source2 = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                             render,
                                                                             uploader,
                                                                             end,
                                                                             &end->bounds);
          }
          break;

        case GSK_VULKAN_OP_BLEND_MODE:
          {
            GskRenderNode *top = gsk_blend_node_get_top_child (op->render.node);
            GskRenderNode *bottom = gsk_blend_node_get_bottom_child (op->render.node);

            op->render.source = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                            render,
                                                                            uploader,
                                                                            top,
                                                                            &top->bounds);
            op->render.source2 = gsk_vulkan_render_pass_get_node_as_texture (self,
                                                                             render,
                                                                             uploader,
                                                                             bottom,
                                                                             &bottom->bounds);
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

gsize
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
        case GSK_VULKAN_OP_SURFACE:
        case GSK_VULKAN_OP_TEXTURE:
          op->render.vertex_count = gsk_vulkan_blend_pipeline_count_vertex_data (GSK_VULKAN_BLEND_PIPELINE (op->render.pipeline));
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

gsize
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
        case GSK_VULKAN_OP_SURFACE:
        case GSK_VULKAN_OP_TEXTURE:
          {
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_blend_pipeline_collect_vertex_data (GSK_VULKAN_BLEND_PIPELINE (op->render.pipeline),
                                                           data + n_bytes + offset,
                                                           &op->render.node->bounds);
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
                                                          gsk_text_node_get_font (op->text.node),
                                                          gsk_text_node_get_glyphs (op->text.node),
                                                          gsk_text_node_get_color (op->text.node),
                                                          gsk_text_node_get_x (op->text.node),
                                                          gsk_text_node_get_y (op->text.node),
                                                          op->text.start_glyph,
                                                          op->text.num_glyphs);
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
                                                                gsk_text_node_get_font (op->text.node),
                                                                gsk_text_node_get_glyphs (op->text.node),
                                                                gsk_text_node_get_x (op->text.node),
                                                                gsk_text_node_get_y (op->text.node),
                                                                op->text.start_glyph,
                                                                op->text.num_glyphs);
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
            GskRenderNode *start = gsk_cross_fade_node_get_start_child (op->render.node);
            GskRenderNode *end = gsk_cross_fade_node_get_end_child (op->render.node);

            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_cross_fade_pipeline_collect_vertex_data (GSK_VULKAN_CROSS_FADE_PIPELINE (op->render.pipeline),
                                                                data + n_bytes + offset,
                                                                &op->render.node->bounds,
                                                                &start->bounds,
                                                                &end->bounds,
                                                                gsk_cross_fade_node_get_progress (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_BLEND_MODE:
          {
            GskRenderNode *top = gsk_blend_node_get_top_child (op->render.node);
            GskRenderNode *bottom = gsk_blend_node_get_bottom_child (op->render.node);

            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_blend_mode_pipeline_collect_vertex_data (GSK_VULKAN_BLEND_MODE_PIPELINE (op->render.pipeline),
                                                                data + n_bytes + offset,
                                                                &op->render.node->bounds,
                                                                &top->bounds,
                                                                &bottom->bounds,
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

GskVulkanBuffer *
gsk_vulkan_render_pass_get_vertex_data (GskVulkanRenderPass *self,
                                        GskVulkanRender     *render)
{
  gsize n_bytes;
  GskVulkanBuffer *buffer;
  guchar *data;

  n_bytes = gsk_vulkan_render_pass_count_vertex_data (self);
  buffer = gsk_vulkan_buffer_new (self->vulkan, n_bytes);
  data = gsk_vulkan_buffer_map (buffer);

  gsk_vulkan_render_pass_collect_vertex_data (self, render, data, 0, n_bytes);

  gsk_vulkan_buffer_unmap (buffer);

  return buffer;
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
        case GSK_VULKAN_OP_SURFACE:
        case GSK_VULKAN_OP_TEXTURE:
        case GSK_VULKAN_OP_OPACITY:
        case GSK_VULKAN_OP_BLUR:
        case GSK_VULKAN_OP_COLOR_MATRIX:
          op->render.descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->render.source);
          break;

        case GSK_VULKAN_OP_TEXT:
        case GSK_VULKAN_OP_COLOR_TEXT:
          op->text.descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->text.source);
          break;

        case GSK_VULKAN_OP_CROSS_FADE:
        case GSK_VULKAN_OP_BLEND_MODE:
          op->render.descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->render.source);
          op->render.descriptor_set_index2 = gsk_vulkan_render_reserve_descriptor_set (render, op->render.source2);
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

void
gsk_vulkan_render_pass_draw (GskVulkanRenderPass     *self,
                             GskVulkanRender         *render,
                             GskVulkanBuffer         *vertex_buffer,
                             guint                    layout_count,
                             VkPipelineLayout        *pipeline_layout,
                             VkCommandBuffer          command_buffer)
{
  GskVulkanPipeline *current_pipeline = NULL;
  gsize current_draw_index = 0;
  GskVulkanOp *op;
  guint i, step;

  for (i = 0; i < self->render_ops->len; i += step)
    {
      op = &g_array_index (self->render_ops, GskVulkanOp, i);
      step = 1;

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
        case GSK_VULKAN_OP_FALLBACK_CLIP:
        case GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP:
        case GSK_VULKAN_OP_SURFACE:
        case GSK_VULKAN_OP_TEXTURE:
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

          current_draw_index += gsk_vulkan_blend_pipeline_draw (GSK_VULKAN_BLEND_PIPELINE (current_pipeline),
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
          for (int i = 0; i < layout_count; i++)
            gsk_vulkan_push_constants_push (&op->constants.constants,
                                            command_buffer,
                                            pipeline_layout[i]);
          break;

        case GSK_VULKAN_OP_CROSS_FADE:
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

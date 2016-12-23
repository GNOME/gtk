#include "config.h"

#include "gskvulkanrenderpassprivate.h"

#include "gskdebugprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderer.h"
#include "gskroundedrectprivate.h"
#include "gskvulkanblendpipelineprivate.h"
#include "gskvulkanclipprivate.h"
#include "gskvulkancolorpipelineprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanpushconstantsprivate.h"
#include "gskvulkanrendererprivate.h"

typedef union _GskVulkanOp GskVulkanOp;
typedef struct _GskVulkanOpRender GskVulkanOpRender;
typedef struct _GskVulkanOpPushConstants GskVulkanOpPushConstants;

typedef enum {
  /* GskVulkanOpRender */
  GSK_VULKAN_OP_FALLBACK,
  GSK_VULKAN_OP_FALLBACK_CLIP,
  GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP,
  GSK_VULKAN_OP_SURFACE,
  GSK_VULKAN_OP_TEXTURE,
  GSK_VULKAN_OP_COLOR,
  /* GskVulkanOpPushConstants */
  GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS
} GskVulkanOpType;

struct _GskVulkanOpRender
{
  GskVulkanOpType      type;
  GskRenderNode       *node; /* node that's the source of this op */
  GskVulkanPipeline   *pipeline; /* pipeline to use */
  GskRoundedRect       clip; /* clip rect (or random memory if not relevant) */
  GskVulkanImage      *source; /* source image to render */
  gsize                vertex_offset; /* offset into vertex buffer */
  gsize                vertex_count; /* number of vertices */
  gsize                descriptor_set_index; /* index into descriptor sets array for the right descriptor set to bind */
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
  GskVulkanOpPushConstants constants;
};

struct _GskVulkanRenderPass
{
  GdkVulkanContext *vulkan;

  GArray *render_ops;
};

GskVulkanRenderPass *
gsk_vulkan_render_pass_new (GdkVulkanContext *context)
{
  GskVulkanRenderPass *self;

  self = g_slice_new0 (GskVulkanRenderPass);
  self->vulkan = g_object_ref (context);
  self->render_ops = g_array_new (FALSE, FALSE, sizeof (GskVulkanOp));

  return self;
}

void
gsk_vulkan_render_pass_free (GskVulkanRenderPass *self)
{
  g_array_unref (self->render_ops);
  g_object_unref (self->vulkan);

  g_slice_free (GskVulkanRenderPass, self);
}

#define FALLBACK(...) G_STMT_START { \
  GSK_NOTE (FALLBACK, g_print (__VA_ARGS__)); \
  goto fallback; \
}G_STMT_END

void
gsk_vulkan_render_pass_add_node (GskVulkanRenderPass           *self,
                                 GskVulkanRender               *render,
                                 const GskVulkanPushConstants  *constants,
                                 const GskVulkanClip           *clip,
                                 GskRenderNode                 *node)
{
  GskVulkanOp op = {
    .type = GSK_VULKAN_OP_FALLBACK,
    .render.node = node
  };

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      return;
    default:
      FALLBACK ("Unsupported node '%s'\n", node->node_class->type_name);

    case GSK_CAIRO_NODE:
      if (gsk_cairo_node_get_surface (node) == NULL)
        return;
      if (!gsk_vulkan_clip_contains_rect (clip, &node->bounds))
        FALLBACK ("Cairo nodes can't deal with clip type %u\n", clip->type);
      op.type = GSK_VULKAN_OP_SURFACE;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_BLIT);
      g_array_append_val (self->render_ops, op);
      return;

    case GSK_TEXTURE_NODE:
      if (!gsk_vulkan_clip_contains_rect (clip, &node->bounds))
        FALLBACK ("Texture nodes can't deal with clip type %u\n", clip->type);
      op.type = GSK_VULKAN_OP_TEXTURE;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_BLIT);
      g_array_append_val (self->render_ops, op);
      return;

    case GSK_COLOR_NODE:
      if (!gsk_vulkan_clip_contains_rect (clip, &node->bounds))
        FALLBACK ("Color nodes can't deal with clip type %u\n", clip->type);
      op.type = GSK_VULKAN_OP_COLOR;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_COLOR);
      g_array_append_val (self->render_ops, op);
      return;

    case GSK_CONTAINER_NODE:
      {
        guint i;

        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          {
            gsk_vulkan_render_pass_add_node (self, render, constants, clip, gsk_container_node_get_child (node, i));
          }
      }
      return;

    case GSK_TRANSFORM_NODE:
      {
        graphene_matrix_t transform;
        GskVulkanClip new_clip;
        graphene_rect_t rect;

        if (!gsk_vulkan_clip_contains_rect (clip, &node->bounds))
          FALLBACK ("Transform nodes can't deal with clip type %u\n", clip->type);

        gsk_transform_node_get_transform (node, &transform);
        op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
        gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
        gsk_vulkan_push_constants_multiply_mvp (&op.constants.constants, &transform);
        g_array_append_val (self->render_ops, op);

        graphene_matrix_transform_bounds (&transform, &clip->rect.bounds, &rect);
        gsk_vulkan_clip_init_empty (&new_clip, &rect);

        gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, &new_clip, gsk_transform_node_get_child (node));
        gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
        g_array_append_val (self->render_ops, op);
      }
      return;

    case GSK_CLIP_NODE:
      {
        GskVulkanClip new_clip;

        if (!gsk_vulkan_clip_intersect_rect (&new_clip, clip, gsk_clip_node_peek_clip (node)))
          FALLBACK ("Failed to find intersection between clip of type %u and rectangle\n", clip->type);
        if (new_clip.type == GSK_VULKAN_CLIP_ALL_CLIPPED)
          return;

        gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, &new_clip, gsk_clip_node_get_child (node));
      }
      return;

    case GSK_ROUNDED_CLIP_NODE:
      {
        GskVulkanClip new_clip;

        if (!gsk_vulkan_clip_intersect_rounded_rect (&new_clip, clip, gsk_rounded_clip_node_peek_clip (node)))
          FALLBACK ("Failed to find intersection between clip of type %u and rounded rectangle\n", clip->type);
        if (new_clip.type == GSK_VULKAN_CLIP_ALL_CLIPPED)
          return;

        gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, &new_clip, gsk_rounded_clip_node_get_child (node));
      }
      return;
    }

  g_assert_not_reached ();
  return;

fallback:
  switch (clip->type)
    {
      case GSK_VULKAN_CLIP_NONE:
        op.type = GSK_VULKAN_OP_FALLBACK;
        break;
      case GSK_VULKAN_CLIP_RECT:
        op.type = GSK_VULKAN_OP_FALLBACK_CLIP;
        gsk_rounded_rect_init_copy (&op.render.clip, &clip->rect);
        break;
      case GSK_VULKAN_CLIP_ROUNDED_CIRCULAR:
      case GSK_VULKAN_CLIP_ROUNDED:
        op.type = GSK_VULKAN_OP_FALLBACK_ROUNDED_CLIP;
        gsk_rounded_rect_init_copy (&op.render.clip, &clip->rect);
        break;
      case GSK_VULKAN_CLIP_ALL_CLIPPED:
      default:
        g_assert_not_reached ();
        return;
    }
  op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_BLIT);
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
  GskVulkanClip clip;

  op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
  gsk_vulkan_push_constants_init (&op.constants.constants, mvp);
  g_array_append_val (self->render_ops, op);

  gsk_vulkan_clip_init_empty (&clip, viewport);

  gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, &clip, node);
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
            cairo_surface_t *surface = gsk_cairo_node_get_surface (op->render.node);
            op->render.source = gsk_vulkan_image_new_from_data (uploader,
                                                                cairo_image_surface_get_data (surface),
                                                                cairo_image_surface_get_width (surface),
                                                                cairo_image_surface_get_height (surface),
                                                                cairo_image_surface_get_stride (surface));
            gsk_vulkan_render_add_cleanup_image (render, op->render.source);
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

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_COLOR:
        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
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

        case GSK_VULKAN_OP_COLOR:
          op->render.vertex_count = gsk_vulkan_color_pipeline_count_vertex_data (GSK_VULKAN_COLOR_PIPELINE (op->render.pipeline));
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

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
          continue;
        }

      g_assert (n_bytes + offset <= total);
    }

  return n_bytes;
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
          op->render.descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->render.source);
          break;

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_COLOR:
        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
          break;
        }
    }
}

void
gsk_vulkan_render_pass_draw (GskVulkanRenderPass     *self,
                             GskVulkanRender         *render,
                             GskVulkanBuffer         *vertex_buffer,
                             GskVulkanPipelineLayout *layout,
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
                                   gsk_vulkan_pipeline_layout_get_pipeline_layout (layout),
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
              if (g_array_index (self->render_ops, GskVulkanOp, i + step).type != GSK_VULKAN_OP_COLOR)
                break;
            }
          current_draw_index += gsk_vulkan_color_pipeline_draw (GSK_VULKAN_COLOR_PIPELINE (current_pipeline),
                                                                command_buffer,
                                                                current_draw_index, step);
          break;

        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
          gsk_vulkan_push_constants_push_vertex (&op->constants.constants,
                                                 command_buffer, 
                                                 gsk_vulkan_pipeline_layout_get_pipeline_layout (layout));
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }
}

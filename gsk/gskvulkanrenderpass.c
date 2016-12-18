#include "config.h"

#include "gskvulkanrenderpassprivate.h"

#include "gskvulkanblendpipelineprivate.h"
#include "gskvulkancolorpipelineprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderer.h"
#include "gskvulkanpushconstantsprivate.h"
#include "gskvulkanrendererprivate.h"

typedef union _GskVulkanOp GskVulkanOp;
typedef struct _GskVulkanOpRender GskVulkanOpRender;
typedef struct _GskVulkanOpPushConstants GskVulkanOpPushConstants;

typedef enum {
  /* GskVulkanOpRender */
  GSK_VULKAN_OP_FALLBACK,
  GSK_VULKAN_OP_SURFACE,
  GSK_VULKAN_OP_TEXTURE,
  GSK_VULKAN_OP_COLOR,
  /* GskVulkanOpPushConstants */
  GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS,
  GSK_VULKAN_OP_PUSH_FRAGMENT_CONSTANTS
} GskVulkanOpType;

struct _GskVulkanOpRender
{
  GskVulkanOpType      type;
  GskRenderNode       *node; /* node that's the source of this op */
  GskVulkanPipeline   *pipeline; /* pipeline to use */
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

void
gsk_vulkan_render_pass_add_node (GskVulkanRenderPass           *self,
                                 GskVulkanRender               *render,
                                 const GskVulkanPushConstants  *constants,
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
      break;

    default:
      op.type = GSK_VULKAN_OP_FALLBACK;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_BLIT);
      g_array_append_val (self->render_ops, op);
      break;

    case GSK_CAIRO_NODE:
      if (gsk_cairo_node_get_surface (node) != NULL)
        {
          op.type = GSK_VULKAN_OP_SURFACE;
          op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_BLIT);
          g_array_append_val (self->render_ops, op);
        }
      break;

    case GSK_TEXTURE_NODE:
      op.type = GSK_VULKAN_OP_TEXTURE;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_BLIT);
      g_array_append_val (self->render_ops, op);
      break;

    case GSK_COLOR_NODE:
      op.type = GSK_VULKAN_OP_PUSH_FRAGMENT_CONSTANTS;
      gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
      gsk_vulkan_push_constants_set_color (&op.constants.constants, gsk_color_node_peek_color (node));
      g_array_append_val (self->render_ops, op);

      op.type = GSK_VULKAN_OP_COLOR;
      op.render.pipeline = gsk_vulkan_render_get_pipeline (render, GSK_VULKAN_PIPELINE_COLOR);
      g_array_append_val (self->render_ops, op);
      break;

    case GSK_CONTAINER_NODE:
      {
        guint i;

        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          {
            gsk_vulkan_render_pass_add_node (self, render, constants, gsk_container_node_get_child (node, i));
          }
      }
      break;
    case GSK_TRANSFORM_NODE:
      {
        graphene_matrix_t transform;

        gsk_transform_node_get_transform (node, &transform);
        op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
        gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
        gsk_vulkan_push_constants_multiply_mvp (&op.constants.constants, &transform);
        g_array_append_val (self->render_ops, op);

        gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, gsk_transform_node_get_child (node));
        gsk_vulkan_push_constants_init_copy (&op.constants.constants, constants);
        g_array_append_val (self->render_ops, op);
      }
      break;

    }
}

void
gsk_vulkan_render_pass_add (GskVulkanRenderPass     *self,
                            GskVulkanRender         *render,
                            const graphene_matrix_t *mvp,
                            GskRenderNode           *node)
{
  GskVulkanOp op = { 0, };

  op.type = GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS;
  gsk_vulkan_push_constants_init (&op.constants.constants, mvp);
  g_array_append_val (self->render_ops, op);

  op.type = GSK_VULKAN_OP_PUSH_FRAGMENT_CONSTANTS;
  g_array_append_val (self->render_ops, op);

  gsk_vulkan_render_pass_add_node (self, render, &op.constants.constants, node);
}

static void
gsk_vulkan_render_pass_upload_fallback (GskVulkanRenderPass  *self,
                                        GskVulkanOpRender    *op,
                                        GskVulkanRender      *render,
                                        GskVulkanUploader    *uploader)
{
  graphene_rect_t bounds;
  cairo_surface_t *surface;
  cairo_t *cr;

  gsk_render_node_get_bounds (op->node, &bounds);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        ceil (bounds.size.width),
                                        ceil (bounds.size.height));
  cr = cairo_create (surface);
  cairo_translate (cr, -bounds.origin.x, -bounds.origin.y);

  gsk_render_node_draw (op->node, cr);
  
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
        case GSK_VULKAN_OP_PUSH_FRAGMENT_CONSTANTS:
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
        case GSK_VULKAN_OP_PUSH_FRAGMENT_CONSTANTS:
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
        case GSK_VULKAN_OP_SURFACE:
        case GSK_VULKAN_OP_TEXTURE:
          {
            graphene_rect_t bounds;

            gsk_render_node_get_bounds (op->render.node, &bounds);
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_blend_pipeline_collect_vertex_data (GSK_VULKAN_BLEND_PIPELINE (op->render.pipeline),
                                                           data + n_bytes + offset,
                                                           &bounds);
            n_bytes += op->render.vertex_count;
          }
          break;

        case GSK_VULKAN_OP_COLOR:
          {
            graphene_rect_t bounds;

            gsk_render_node_get_bounds (op->render.node, &bounds);
            op->render.vertex_offset = offset + n_bytes;
            gsk_vulkan_color_pipeline_collect_vertex_data (GSK_VULKAN_COLOR_PIPELINE (op->render.pipeline),
                                                           data + n_bytes + offset,
                                                           &bounds,
                                                           gsk_color_node_peek_color (op->render.node));
            n_bytes += op->render.vertex_count;
          }
          break;

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
        case GSK_VULKAN_OP_PUSH_FRAGMENT_CONSTANTS:
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
        case GSK_VULKAN_OP_SURFACE:
        case GSK_VULKAN_OP_TEXTURE:
          op->render.descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->render.source);
          break;

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_COLOR:
        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
        case GSK_VULKAN_OP_PUSH_FRAGMENT_CONSTANTS:
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
  guint i;

  for (i = 0; i < self->render_ops->len; i++)
    {
      op = &g_array_index (self->render_ops, GskVulkanOp, i);

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
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

          current_draw_index += gsk_vulkan_color_pipeline_draw (GSK_VULKAN_COLOR_PIPELINE (current_pipeline),
                                                                command_buffer,
                                                                current_draw_index, 1);
          break;

        case GSK_VULKAN_OP_PUSH_VERTEX_CONSTANTS:
          gsk_vulkan_push_constants_push_vertex (&op->constants.constants,
                                                 command_buffer, 
                                                 gsk_vulkan_pipeline_layout_get_pipeline_layout (layout));
          break;

        case GSK_VULKAN_OP_PUSH_FRAGMENT_CONSTANTS:
          gsk_vulkan_push_constants_push_fragment (&op->constants.constants,
                                                   command_buffer, 
                                                   gsk_vulkan_pipeline_layout_get_pipeline_layout (layout));
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }
}

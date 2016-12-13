#include "config.h"

#include "gskvulkanrenderpassprivate.h"

#include "gskvulkanimageprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderer.h"
#include "gskvulkanrendererprivate.h"

typedef struct _GskVulkanRenderOp GskVulkanRenderOp;

typedef enum {
  GSK_VULKAN_OP_FALLBACK,
  GSK_VULKAN_OP_SURFACE,
  GSK_VULKAN_OP_TEXTURE,
  GSK_VULKAN_OP_BIND_MVP
} GskVulkanOpType;

struct _GskVulkanRenderOp
{
  GskVulkanOpType      type;
  GskRenderNode       *node; /* node that's the source of this op */
  GskVulkanImage      *source; /* source image to render */
  graphene_matrix_t    mvp; /* new mvp to set */
  gsize                vertex_offset; /* offset into vertex buffer */
  gsize                vertex_count; /* number of vertices */
  gsize                descriptor_set_index; /* index into descriptor sets array for the right descriptor set to bind */
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
  self->render_ops = g_array_new (FALSE, FALSE, sizeof (GskVulkanRenderOp));

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
gsk_vulkan_render_pass_add_node (GskVulkanRenderPass     *self,
                                 GskVulkanRender         *render,
                                 const graphene_matrix_t *mvp,
                                 GskRenderNode           *node)
{
  GskVulkanRenderOp op = {
    .type = GSK_VULKAN_OP_FALLBACK,
    .node = node
  };

  if (gsk_render_node_get_opacity (node) < 1.0)
    goto fallback;

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      break;

    case GSK_CAIRO_NODE:
      op.type = GSK_VULKAN_OP_SURFACE;
      g_array_append_val (self->render_ops, op);
      break;

    case GSK_TEXTURE_NODE:
      op.type = GSK_VULKAN_OP_TEXTURE;
      g_array_append_val (self->render_ops, op);
      break;

    case GSK_CONTAINER_NODE:
      {
        guint i;

        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          {
            gsk_vulkan_render_pass_add_node (self, render, mvp, gsk_container_node_get_child (node, i));
          }
      }
      break;
    case GSK_TRANSFORM_NODE:
      {
        graphene_matrix_t transform;

        op.type = GSK_VULKAN_OP_BIND_MVP;
        gsk_transform_node_get_transform (node, &transform);
        graphene_matrix_multiply (&transform, mvp, &op.mvp);
        g_array_append_val (self->render_ops, op);
        gsk_vulkan_render_pass_add_node (self, render, &op.mvp, gsk_transform_node_get_child (node));
        graphene_matrix_init_from_matrix (&op.mvp, mvp);
        g_array_append_val (self->render_ops, op);
      }
      break;

    }

  return;

fallback:
  g_array_append_val (self->render_ops, op);
}

void
gsk_vulkan_render_pass_add (GskVulkanRenderPass     *self,
                            GskVulkanRender         *render,
                            const graphene_matrix_t *mvp,
                            GskRenderNode           *node)
{
  GskVulkanRenderOp op = {
    .type = GSK_VULKAN_OP_BIND_MVP,
    .mvp = *mvp
  };

  g_array_append_val (self->render_ops, op);

  gsk_vulkan_render_pass_add_node (self, render, mvp, node);
}

static void
gsk_vulkan_render_pass_upload_fallback (GskVulkanRenderPass *self,
                                        GskVulkanRenderOp   *op,
                                        GskVulkanRender     *render,
                                        VkCommandBuffer      command_buffer)
{
  graphene_rect_t bounds;
  cairo_surface_t *surface;
  cairo_t *cr;

  gsk_render_node_get_bounds (op->node, &bounds);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        ceil (bounds.size.width),
                                        ceil (bounds.size.height));
  cr = cairo_create (surface);
  cairo_translate (cr, bounds.origin.x, bounds.origin.y);

  gsk_render_node_draw (op->node, cr);
  
  cairo_destroy (cr);

  op->source = gsk_vulkan_image_new_from_data (self->vulkan,
                                               command_buffer,
                                               cairo_image_surface_get_data (surface),
                                               cairo_image_surface_get_width (surface),
                                               cairo_image_surface_get_height (surface),
                                               cairo_image_surface_get_stride (surface));

  cairo_surface_destroy (surface);

  gsk_vulkan_render_add_cleanup_image (render, op->source);
}

void
gsk_vulkan_render_pass_upload (GskVulkanRenderPass *self,
                               GskVulkanRender     *render,
                               VkCommandBuffer      command_buffer)
{
  GskVulkanRenderOp *op;
  guint i;

  for (i = 0; i < self->render_ops->len; i++)
    {
      op = &g_array_index (self->render_ops, GskVulkanRenderOp, i);

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
          gsk_vulkan_render_pass_upload_fallback (self, op, render, command_buffer);
          break;

        case GSK_VULKAN_OP_SURFACE:
          {
            cairo_surface_t *surface = gsk_cairo_node_get_surface (op->node);
            op->source = gsk_vulkan_image_new_from_data (self->vulkan,
                                                         command_buffer,
                                                         cairo_image_surface_get_data (surface),
                                                         cairo_image_surface_get_width (surface),
                                                         cairo_image_surface_get_height (surface),
                                                         cairo_image_surface_get_stride (surface));
            gsk_vulkan_render_add_cleanup_image (render, op->source);
          }
          break;

        case GSK_VULKAN_OP_TEXTURE:
          {
            op->source = gsk_vulkan_renderer_ref_texture_image (GSK_VULKAN_RENDERER (gsk_vulkan_render_get_renderer (render)),
                                                                gsk_texture_node_get_texture (op->node),
                                                                command_buffer);
            gsk_vulkan_render_add_cleanup_image (render, op->source);
          }
          break;

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_BIND_MVP:
          break;
        }
    }
}

gsize
gsk_vulkan_render_pass_count_vertices (GskVulkanRenderPass *self)
{
  return self->render_ops->len * 6;
}

static gsize
gsk_vulkan_render_op_collect_vertices (GskVulkanRenderOp *op,
                                       GskVulkanVertex   *vertices)
{
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (op->node, &bounds);

  vertices[0] = (GskVulkanVertex) { bounds.origin.x,                     bounds.origin.y,                      0.0, 0.0 };
  vertices[1] = (GskVulkanVertex) { bounds.origin.x + bounds.size.width, bounds.origin.y,                      1.0, 0.0 };
  vertices[2] = (GskVulkanVertex) { bounds.origin.x,                     bounds.origin.y + bounds.size.height, 0.0, 1.0 };
  vertices[3] = (GskVulkanVertex) { bounds.origin.x,                     bounds.origin.y + bounds.size.height, 0.0, 1.0 };
  vertices[4] = (GskVulkanVertex) { bounds.origin.x + bounds.size.width, bounds.origin.y,                      1.0, 0.0 };
  vertices[5] = (GskVulkanVertex) { bounds.origin.x + bounds.size.width, bounds.origin.y + bounds.size.height, 1.0, 1.0 };

  return 6;
}

gsize
gsk_vulkan_render_pass_collect_vertices (GskVulkanRenderPass *self,
                                         GskVulkanVertex     *vertices,
                                         gsize                offset,
                                         gsize                total)
{
  GskVulkanRenderOp *op;
  gsize n;
  guint i;

  n = 0;
  for (i = 0; i < self->render_ops->len; i++)
    {
      op = &g_array_index (self->render_ops, GskVulkanRenderOp, i);

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
        case GSK_VULKAN_OP_SURFACE:
        case GSK_VULKAN_OP_TEXTURE:
          op->vertex_offset = offset + n;
          op->vertex_count = gsk_vulkan_render_op_collect_vertices (op, vertices + n + offset);
          break;

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_BIND_MVP:
          op->vertex_offset = 0;
          op->vertex_count = 0;
          break;
        }

      n += op->vertex_count;
      g_assert (n + offset <= total);
    }

  return n;
}

void
gsk_vulkan_render_pass_reserve_descriptor_sets (GskVulkanRenderPass *self,
                                                GskVulkanRender     *render)
{
  GskVulkanRenderOp *op;
  guint i;

  for (i = 0; i < self->render_ops->len; i++)
    {
      op = &g_array_index (self->render_ops, GskVulkanRenderOp, i);

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
        case GSK_VULKAN_OP_SURFACE:
        case GSK_VULKAN_OP_TEXTURE:
          op->descriptor_set_index = gsk_vulkan_render_reserve_descriptor_set (render, op->source);
          break;

        default:
          g_assert_not_reached ();
        case GSK_VULKAN_OP_BIND_MVP:
          break;
        }
    }
}

void
gsk_vulkan_render_pass_draw (GskVulkanRenderPass     *self,
                             GskVulkanRender         *render,
                             GskVulkanPipeline       *pipeline,
                             VkCommandBuffer          command_buffer)
{
  GskVulkanRenderOp *op;
  float float_matrix[16];
  guint i;

  for (i = 0; i < self->render_ops->len; i++)
    {
      op = &g_array_index (self->render_ops, GskVulkanRenderOp, i);

      switch (op->type)
        {
        case GSK_VULKAN_OP_FALLBACK:
        case GSK_VULKAN_OP_SURFACE:
        case GSK_VULKAN_OP_TEXTURE:
          vkCmdBindDescriptorSets (command_buffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   gsk_vulkan_pipeline_get_pipeline_layout (pipeline),
                                   0,
                                   1,
                                   (VkDescriptorSet[1]) {
                                       gsk_vulkan_render_get_descriptor_set (render, op->descriptor_set_index)
                                   },
                                   0,
                                   NULL);

          vkCmdDraw (command_buffer,
                     op->vertex_count, 1,
                     op->vertex_offset, 0);
          break;

        case GSK_VULKAN_OP_BIND_MVP:
          graphene_matrix_to_float (&op->mvp, float_matrix);
          vkCmdPushConstants (command_buffer,
                              gsk_vulkan_pipeline_get_pipeline_layout (pipeline),
                              VK_SHADER_STAGE_VERTEX_BIT,
                              0,
                              sizeof (float_matrix),
                              &float_matrix);

          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }
}

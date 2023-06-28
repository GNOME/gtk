#include "config.h"

#include "gskvulkanoffscreenopprivate.h"

#include "gskrendernodeprivate.h"

#include "gdk/gdkvulkancontextprivate.h"

typedef struct _GskVulkanOffscreenOp GskVulkanOffscreenOp;

struct _GskVulkanOffscreenOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  GskVulkanRenderPass *render_pass;
};

static void
gsk_vulkan_offscreen_op_finish (GskVulkanOp *op)
{
  GskVulkanOffscreenOp *self = (GskVulkanOffscreenOp *) op;

  g_object_unref (self->image);
  gsk_vulkan_render_pass_free (self->render_pass);
}

static void
gsk_vulkan_offscreen_op_upload (GskVulkanOp           *op,
                                GskVulkanRenderPass   *pass,
                                GskVulkanRender       *render,
                                GskVulkanUploader     *uploader,
                                const graphene_rect_t *clip,
                                const graphene_vec2_t *scale)
{
  GskVulkanOffscreenOp *self = (GskVulkanOffscreenOp *) op;

  gsk_vulkan_render_pass_upload (self->render_pass, render, uploader);
}

static gsize
gsk_vulkan_offscreen_op_count_vertex_data (GskVulkanOp *op,
                                           gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_offscreen_op_collect_vertex_data (GskVulkanOp         *op,
                                             GskVulkanRenderPass *pass,
                                             GskVulkanRender     *render,
                                             guchar              *data)
{
}

static void
gsk_vulkan_offscreen_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                 GskVulkanRender *render)
{
  GskVulkanOffscreenOp *self = (GskVulkanOffscreenOp *) op;

  gsk_vulkan_render_pass_reserve_descriptor_sets (self->render_pass, render);
}

static VkPipeline
gsk_vulkan_offscreen_op_get_pipeline (GskVulkanOp *op)
{
  return NULL;
}

static void
gsk_vulkan_offscreen_op_command (GskVulkanOp      *op,
                                 GskVulkanRender  *render,
                                 VkPipelineLayout  pipeline_layout,
                                 VkCommandBuffer   command_buffer)
{
  GskVulkanOffscreenOp *self = (GskVulkanOffscreenOp *) op;

  gsk_vulkan_render_draw_pass (render, self->render_pass, VK_NULL_HANDLE);
}

static const GskVulkanOpClass GSK_VULKAN_OFFSCREEN_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanOffscreenOp),
  gsk_vulkan_offscreen_op_finish,
  gsk_vulkan_offscreen_op_upload,
  gsk_vulkan_offscreen_op_count_vertex_data,
  gsk_vulkan_offscreen_op_collect_vertex_data,
  gsk_vulkan_offscreen_op_reserve_descriptor_sets,
  gsk_vulkan_offscreen_op_get_pipeline,
  gsk_vulkan_offscreen_op_command
};

GskVulkanImage *
gsk_vulkan_offscreen_op (GskVulkanRenderPass   *render_pass,
                         GdkVulkanContext      *context,
                         GskVulkanRender       *render,
                         const graphene_vec2_t *scale,
                         const graphene_rect_t *viewport,
                         VkSemaphore            signal_semaphore,
                         GskRenderNode         *node)
{
  GskVulkanOffscreenOp *self;
  graphene_rect_t view;
  cairo_region_t *clip;
  float scale_x, scale_y;

  scale_x = graphene_vec2_get_x (scale);
  scale_y = graphene_vec2_get_y (scale);
  view = GRAPHENE_RECT_INIT (scale_x * viewport->origin.x,
                             scale_y * viewport->origin.y,
                             ceil (scale_x * viewport->size.width),
                             ceil (scale_y * viewport->size.height));

  self = (GskVulkanOffscreenOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_OFFSCREEN_OP_CLASS);

  self->image = gsk_vulkan_image_new_for_offscreen (context,
                                                    gdk_vulkan_context_get_offscreen_format (context,
                                                        gsk_render_node_get_preferred_depth (node)),
                                                    view.size.width, view.size.height);

  clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                          0, 0,
                                          gsk_vulkan_image_get_width (self->image),
                                          gsk_vulkan_image_get_height (self->image)
                                        });

  self->render_pass = gsk_vulkan_render_pass_new (context,
                                                  self->image,
                                                  scale,
                                                  &view,
                                                  clip,
                                                  signal_semaphore);

  cairo_region_destroy (clip);

  gsk_vulkan_render_pass_add (self->render_pass, render, node);

  return self->image;
}

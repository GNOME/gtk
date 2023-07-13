#include "config.h"

#include "gskvulkanrenderpassopprivate.h"

#include "gskrendernodeprivate.h"
#include "gskvulkanprivate.h"

#include "gdk/gdkvulkancontextprivate.h"

typedef struct _GskVulkanRenderPassOp GskVulkanRenderPassOp;

struct _GskVulkanRenderPassOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  GskVulkanRenderPass *render_pass;
};

static void
gsk_vulkan_render_pass_op_finish (GskVulkanOp *op)
{
  GskVulkanRenderPassOp *self = (GskVulkanRenderPassOp *) op;

  g_object_unref (self->image);
  gsk_vulkan_render_pass_free (self->render_pass);
}

static void
gsk_vulkan_render_pass_op_print (GskVulkanOp *op,
                                 GString     *string,
                                 guint        indent)
{
  GskVulkanRenderPassOp *self = (GskVulkanRenderPassOp *) op;

  print_indent (string, indent);
  g_string_append_printf (string, "begin-render-pass %zux%zu ",
                          gsk_vulkan_image_get_width (self->image),
                          gsk_vulkan_image_get_height (self->image));
  print_newline (string);
}

static void
gsk_vulkan_render_pass_op_upload (GskVulkanOp       *op,
                                  GskVulkanUploader *uploader)
{
}

static gsize
gsk_vulkan_render_pass_op_count_vertex_data (GskVulkanOp *op,
                                             gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_render_pass_op_collect_vertex_data (GskVulkanOp *op,
                                               guchar      *data)
{
}

static void
gsk_vulkan_render_pass_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                   GskVulkanRender *render)
{
}

static GskVulkanOp *
gsk_vulkan_render_pass_op_command (GskVulkanOp      *op,
                                   GskVulkanRender  *render,
                                   VkPipelineLayout  pipeline_layout,
                                   VkCommandBuffer   command_buffer)
{
  GskVulkanRenderPassOp *self = (GskVulkanRenderPassOp *) op;
  VkPipeline current_pipeline = VK_NULL_HANDLE;
  const GskVulkanOpClass *current_pipeline_class = NULL;
  const char *current_pipeline_clip_type = NULL;
  VkRenderPass vk_render_pass;

  vk_render_pass = gsk_vulkan_render_pass_begin_draw (self->render_pass, render, pipeline_layout, command_buffer);

  op = op->next;
  while (op && op->op_class->stage != GSK_VULKAN_STAGE_END_PASS)
    {
      if (op->op_class->shader_name &&
          (op->op_class != current_pipeline_class ||
           current_pipeline_clip_type != op->clip_type))
        {
          current_pipeline = gsk_vulkan_render_get_pipeline (render,
                                                             op->op_class,
                                                             op->clip_type,
                                                             gsk_vulkan_image_get_vk_format (self->image),
                                                             vk_render_pass);
          vkCmdBindPipeline (command_buffer,
                             VK_PIPELINE_BIND_POINT_GRAPHICS,
                             current_pipeline);
          current_pipeline_class = op->op_class;
          current_pipeline_clip_type = op->clip_type;
        }

      op = gsk_vulkan_op_command (op, render, pipeline_layout, command_buffer);
    }

  if (op && op->op_class->stage == GSK_VULKAN_STAGE_END_PASS)
    op = gsk_vulkan_op_command (op, render, pipeline_layout, command_buffer);
  else
    gsk_vulkan_render_pass_end_draw (self->render_pass, render, pipeline_layout, command_buffer);

  return op;
}

static const GskVulkanOpClass GSK_VULKAN_RENDER_PASS_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanRenderPassOp),
  GSK_VULKAN_STAGE_BEGIN_PASS,
  NULL,
  NULL,
  gsk_vulkan_render_pass_op_finish,
  gsk_vulkan_render_pass_op_print,
  gsk_vulkan_render_pass_op_upload,
  gsk_vulkan_render_pass_op_count_vertex_data,
  gsk_vulkan_render_pass_op_collect_vertex_data,
  gsk_vulkan_render_pass_op_reserve_descriptor_sets,
  gsk_vulkan_render_pass_op_command
};

typedef struct _GskVulkanRenderPassEndOp GskVulkanRenderPassEndOp;

struct _GskVulkanRenderPassEndOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
};

static void
gsk_vulkan_render_pass_end_op_finish (GskVulkanOp *op)
{
  GskVulkanRenderPassEndOp *self = (GskVulkanRenderPassEndOp *) op;

  g_object_unref (self->image);
}

static void
gsk_vulkan_render_pass_end_op_print (GskVulkanOp *op,
                                     GString     *string,
                                     guint        indent)
{
  GskVulkanRenderPassEndOp *self = (GskVulkanRenderPassEndOp *) op;

  print_indent (string, indent);
  g_string_append_printf (string, "end-render-pass ");
  print_image (string, self->image);
  print_newline (string);
}

static void
gsk_vulkan_render_pass_end_op_upload (GskVulkanOp       *op,
                                      GskVulkanUploader *uploader)
{
}

static gsize
gsk_vulkan_render_pass_end_op_count_vertex_data (GskVulkanOp *op,
                                                 gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_render_pass_end_op_collect_vertex_data (GskVulkanOp *op,
                                                   guchar      *data)
{
}

static void
gsk_vulkan_render_pass_end_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                       GskVulkanRender *render)
{
}

static GskVulkanOp *
gsk_vulkan_render_pass_end_op_command (GskVulkanOp      *op,
                                       GskVulkanRender  *render,
                                       VkPipelineLayout  pipeline_layout,
                                       VkCommandBuffer   command_buffer)
{
  vkCmdEndRenderPass (command_buffer);

  return op->next;
}

static const GskVulkanOpClass GSK_VULKAN_RENDER_PASS_END_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanRenderPassEndOp),
  GSK_VULKAN_STAGE_END_PASS,
  NULL,
  NULL,
  gsk_vulkan_render_pass_end_op_finish,
  gsk_vulkan_render_pass_end_op_print,
  gsk_vulkan_render_pass_end_op_upload,
  gsk_vulkan_render_pass_end_op_count_vertex_data,
  gsk_vulkan_render_pass_end_op_collect_vertex_data,
  gsk_vulkan_render_pass_end_op_reserve_descriptor_sets,
  gsk_vulkan_render_pass_end_op_command
};

void
gsk_vulkan_render_pass_op (GskVulkanRender       *render,
                           GdkVulkanContext      *context,
                           GskVulkanImage        *image,
                           cairo_region_t        *clip,
                           const graphene_vec2_t *scale,
                           const graphene_rect_t *viewport,
                           GskRenderNode         *node,
                           gboolean               is_root)
{
  GskVulkanRenderPassOp *self;
  GskVulkanRenderPassEndOp *end;

  self = (GskVulkanRenderPassOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_RENDER_PASS_OP_CLASS);

  self->image = image;

  self->render_pass = gsk_vulkan_render_pass_new (context,
                                                  render,
                                                  self->image,
                                                  scale,
                                                  viewport,
                                                  clip,
                                                  node,
                                                  is_root);

  /* This invalidates the self pointer */
  gsk_vulkan_render_pass_add (self->render_pass, render, node);

  end = (GskVulkanRenderPassEndOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_RENDER_PASS_END_OP_CLASS);

  end->image = g_object_ref (image);
}

GskVulkanImage *
gsk_vulkan_render_pass_op_offscreen (GskVulkanRender       *render,
                                     GdkVulkanContext      *context,
                                     const graphene_vec2_t *scale,
                                     const graphene_rect_t *viewport,
                                     GskRenderNode         *node)
{
  graphene_rect_t view;
  GskVulkanImage *image;
  cairo_region_t *clip;
  float scale_x, scale_y;

  scale_x = graphene_vec2_get_x (scale);
  scale_y = graphene_vec2_get_y (scale);
  view = GRAPHENE_RECT_INIT (scale_x * viewport->origin.x,
                             scale_y * viewport->origin.y,
                             ceil (scale_x * viewport->size.width),
                             ceil (scale_y * viewport->size.height));

  image = gsk_vulkan_image_new_for_offscreen (context,
                                              gdk_vulkan_context_get_offscreen_format (context,
                                                  gsk_render_node_get_preferred_depth (node)),
                                              view.size.width, view.size.height);

  clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                          0, 0,
                                          gsk_vulkan_image_get_width (image),
                                          gsk_vulkan_image_get_height (image)
                                        });

  gsk_vulkan_render_pass_op (render,
                             context,
                             image,
                             clip,
                             scale,
                             &view,
                             node,
                             FALSE);

  cairo_region_destroy (clip);

  return image;
}

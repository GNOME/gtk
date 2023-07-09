#include "config.h"

#include "gskvulkanoffscreenopprivate.h"

#include "gskrendernodeprivate.h"
#include "gskvulkanprivate.h"

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
gsk_vulkan_offscreen_op_print (GskVulkanOp *op,
                               GString     *string,
                               guint        indent)
{
  GskVulkanOffscreenOp *self = (GskVulkanOffscreenOp *) op;

  print_indent (string, indent);
  g_string_append_printf (string, "offscreen %zux%zu ",
                          gsk_vulkan_image_get_width (self->image),
                          gsk_vulkan_image_get_height (self->image));
  print_newline (string);
}

static void
gsk_vulkan_offscreen_op_upload (GskVulkanOp       *op,
                                GskVulkanUploader *uploader)
{
}

static gsize
gsk_vulkan_offscreen_op_count_vertex_data (GskVulkanOp *op,
                                           gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_offscreen_op_collect_vertex_data (GskVulkanOp *op,
                                             guchar      *data)
{
}

static void
gsk_vulkan_offscreen_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                 GskVulkanRender *render)
{
}

static GskVulkanOp *
gsk_vulkan_offscreen_op_command (GskVulkanOp      *op,
                                 GskVulkanRender  *render,
                                 VkPipelineLayout  pipeline_layout,
                                 VkCommandBuffer   command_buffer)
{
  GskVulkanOffscreenOp *self = (GskVulkanOffscreenOp *) op;

  return gsk_vulkan_render_draw_pass (render, self->render_pass, op->next);
}

static const GskVulkanOpClass GSK_VULKAN_OFFSCREEN_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanOffscreenOp),
  GSK_VULKAN_STAGE_BEGIN_PASS,
  NULL,
  NULL,
  gsk_vulkan_offscreen_op_finish,
  gsk_vulkan_offscreen_op_print,
  gsk_vulkan_offscreen_op_upload,
  gsk_vulkan_offscreen_op_count_vertex_data,
  gsk_vulkan_offscreen_op_collect_vertex_data,
  gsk_vulkan_offscreen_op_reserve_descriptor_sets,
  gsk_vulkan_offscreen_op_command
};

typedef struct _GskVulkanOffscreenEndOp GskVulkanOffscreenEndOp;

struct _GskVulkanOffscreenEndOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
};

static void
gsk_vulkan_offscreen_end_op_finish (GskVulkanOp *op)
{
  GskVulkanOffscreenEndOp *self = (GskVulkanOffscreenEndOp *) op;

  g_object_unref (self->image);
}

static void
gsk_vulkan_offscreen_end_op_print (GskVulkanOp *op,
                                   GString     *string,
                                   guint        indent)
{
  GskVulkanOffscreenEndOp *self = (GskVulkanOffscreenEndOp *) op;

  print_indent (string, indent);
  g_string_append_printf (string, "end offscreen ");
  print_image (string, self->image);
  print_newline (string);
}

static void
gsk_vulkan_offscreen_end_op_upload (GskVulkanOp       *op,
                                    GskVulkanUploader *uploader)
{
}

static gsize
gsk_vulkan_offscreen_end_op_count_vertex_data (GskVulkanOp *op,
                                               gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_offscreen_end_op_collect_vertex_data (GskVulkanOp *op,
                                                 guchar      *data)
{
}

static void
gsk_vulkan_offscreen_end_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                     GskVulkanRender *render)
{
}

static GskVulkanOp *
gsk_vulkan_offscreen_end_op_command (GskVulkanOp      *op,
                                     GskVulkanRender  *render,
                                     VkPipelineLayout  pipeline_layout,
                                     VkCommandBuffer   command_buffer)
{
  vkCmdEndRenderPass (command_buffer);

  return op->next;
}

static const GskVulkanOpClass GSK_VULKAN_OFFSCREEN_END_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanOffscreenEndOp),
  GSK_VULKAN_STAGE_END_PASS,
  NULL,
  NULL,
  gsk_vulkan_offscreen_end_op_finish,
  gsk_vulkan_offscreen_end_op_print,
  gsk_vulkan_offscreen_end_op_upload,
  gsk_vulkan_offscreen_end_op_count_vertex_data,
  gsk_vulkan_offscreen_end_op_collect_vertex_data,
  gsk_vulkan_offscreen_end_op_reserve_descriptor_sets,
  gsk_vulkan_offscreen_end_op_command
};

GskVulkanImage *
gsk_vulkan_offscreen_op (GskVulkanRender       *render,
                         GdkVulkanContext      *context,
                         const graphene_vec2_t *scale,
                         const graphene_rect_t *viewport,
                         GskRenderNode         *node)
{
  GskVulkanOffscreenOp *self;
  GskVulkanOffscreenEndOp *end;
  GskVulkanImage *image;
  graphene_rect_t view;
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

  self = (GskVulkanOffscreenOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_OFFSCREEN_OP_CLASS);

  self->image = image;

  clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                          0, 0,
                                          gsk_vulkan_image_get_width (self->image),
                                          gsk_vulkan_image_get_height (self->image)
                                        });

  self->render_pass = gsk_vulkan_render_pass_new (context,
                                                  render,
                                                  self->image,
                                                  scale,
                                                  &view,
                                                  clip,
                                                  node,
                                                  FALSE);

  cairo_region_destroy (clip);

  /* This invalidates the self pointer */
  gsk_vulkan_render_pass_add (self->render_pass, render, node);

  end = (GskVulkanOffscreenEndOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_OFFSCREEN_END_OP_CLASS);

  end->image = g_object_ref (image);

  return self->image;
}

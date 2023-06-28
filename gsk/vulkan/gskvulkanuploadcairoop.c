#include "config.h"

#include "gskvulkanuploadcairoopprivate.h"

typedef struct _GskVulkanUploadCairoOp GskVulkanUploadCairoOp;

struct _GskVulkanUploadCairoOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  GskRenderNode *node;
  graphene_rect_t viewport;
};

static void
gsk_vulkan_upload_cairo_op_finish (GskVulkanOp *op)
{
  GskVulkanUploadCairoOp *self = (GskVulkanUploadCairoOp *) op;

  g_object_unref (self->image);
  gsk_render_node_unref (self->node);
}

static void
gsk_vulkan_upload_cairo_op_upload (GskVulkanOp           *op,
                                   GskVulkanRenderPass   *pass,
                                   GskVulkanRender       *render,
                                   GskVulkanUploader     *uploader,
                                   const graphene_rect_t *clip,
                                   const graphene_vec2_t *scale)
{
  GskVulkanUploadCairoOp *self = (GskVulkanUploadCairoOp *) op;
  GskVulkanImageMap map;
  cairo_surface_t *surface;
  cairo_t *cr;
  int width, height;

  width = gsk_vulkan_image_get_width (self->image);
  height = gsk_vulkan_image_get_height (self->image);

  gsk_vulkan_image_map_memory (self->image, uploader, GSK_VULKAN_WRITE, &map);
  surface = cairo_image_surface_create_for_data (map.data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width, height,
                                                 map.stride);
  cairo_surface_set_device_scale (surface,
                                  width / self->viewport.size.width,
                                  height / self->viewport.size.height);
  cr = cairo_create (surface);
  cairo_translate (cr, -self->viewport.origin.x, -self->viewport.origin.y);

  gsk_render_node_draw (self->node, cr);

  cairo_destroy (cr);

  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);

  gsk_vulkan_image_unmap_memory (self->image, uploader, &map);
}

static gsize
gsk_vulkan_upload_cairo_op_count_vertex_data (GskVulkanOp *op,
                                              gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_upload_cairo_op_collect_vertex_data (GskVulkanOp         *op,
                                                GskVulkanRenderPass *pass,
                                                GskVulkanRender     *render,
                                                guchar              *data)
{
}

static void
gsk_vulkan_upload_cairo_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                    GskVulkanRender *render)
{
}

static VkPipeline
gsk_vulkan_upload_cairo_op_get_pipeline (GskVulkanOp *op)
{
  return NULL;
}

static void
gsk_vulkan_upload_cairo_op_command (GskVulkanOp      *op,
                                    GskVulkanRender  *render,
                                    VkPipelineLayout  pipeline_layout,
                                    VkCommandBuffer   command_buffer)
{
}

static const GskVulkanOpClass GSK_VULKAN_UPLOAD_CAIRO_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanUploadCairoOp),
  gsk_vulkan_upload_cairo_op_finish,
  gsk_vulkan_upload_cairo_op_upload,
  gsk_vulkan_upload_cairo_op_count_vertex_data,
  gsk_vulkan_upload_cairo_op_collect_vertex_data,
  gsk_vulkan_upload_cairo_op_reserve_descriptor_sets,
  gsk_vulkan_upload_cairo_op_get_pipeline,
  gsk_vulkan_upload_cairo_op_command
};

GskVulkanImage *
gsk_vulkan_upload_cairo_op (GskVulkanRenderPass   *render_pass,
                            GdkVulkanContext      *context,
                            GskRenderNode         *node,
                            const graphene_vec2_t *scale,
                            const graphene_rect_t *viewport)
{
  GskVulkanUploadCairoOp *self;

  self = (GskVulkanUploadCairoOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_UPLOAD_CAIRO_OP_CLASS);

  self->node = gsk_render_node_ref (node);
  self->image = gsk_vulkan_image_new_for_upload (context,
                                                 GDK_MEMORY_DEFAULT,
                                                 ceil (graphene_vec2_get_x (scale) * viewport->size.width),
                                                 ceil (graphene_vec2_get_y (scale) * viewport->size.height));
  self->viewport = *viewport;

  return self->image;
}

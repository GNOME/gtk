#include "config.h"

#include "gskvulkanuploadopprivate.h"

typedef struct _GskVulkanUploadOp GskVulkanUploadOp;

struct _GskVulkanUploadOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  GdkTexture *texture;
};

static void
gsk_vulkan_upload_op_finish (GskVulkanOp *op)
{
  GskVulkanUploadOp *self = (GskVulkanUploadOp *) op;

  g_object_unref (self->image);
  g_object_unref (self->texture);
}

static void
gsk_vulkan_upload_op_upload (GskVulkanOp           *op,
                             GskVulkanRenderPass   *pass,
                             GskVulkanRender       *render,
                             GskVulkanUploader     *uploader,
                             const graphene_rect_t *clip,
                             const graphene_vec2_t *scale)
{
  GskVulkanUploadOp *self = (GskVulkanUploadOp *) op;
  GdkTextureDownloader *downloader;
  GskVulkanImageMap map;

  downloader = gdk_texture_downloader_new (self->texture);
  gdk_texture_downloader_set_format (downloader, gsk_vulkan_image_get_format (self->image));
  gsk_vulkan_image_map_memory (self->image, uploader, GSK_VULKAN_WRITE, &map);
  gdk_texture_downloader_download_into (downloader, map.data, map.stride);
  gsk_vulkan_image_unmap_memory (self->image, uploader, &map);
  gdk_texture_downloader_free (downloader);
}

static gsize
gsk_vulkan_upload_op_count_vertex_data (GskVulkanOp *op,
                                        gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_upload_op_collect_vertex_data (GskVulkanOp         *op,
                                          GskVulkanRenderPass *pass,
                                          GskVulkanRender     *render,
                                          guchar              *data)
{
}

static void
gsk_vulkan_upload_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                              GskVulkanRender *render)
{
}

static VkPipeline
gsk_vulkan_upload_op_get_pipeline (GskVulkanOp *op)
{
  return NULL;
}

static void
gsk_vulkan_upload_op_command (GskVulkanOp      *op,
                              GskVulkanRender  *render,
                              VkPipelineLayout  pipeline_layout,
                              VkCommandBuffer   command_buffer)
{
}

static const GskVulkanOpClass GSK_VULKAN_UPLOAD_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanUploadOp),
  gsk_vulkan_upload_op_finish,
  gsk_vulkan_upload_op_upload,
  gsk_vulkan_upload_op_count_vertex_data,
  gsk_vulkan_upload_op_collect_vertex_data,
  gsk_vulkan_upload_op_reserve_descriptor_sets,
  gsk_vulkan_upload_op_get_pipeline,
  gsk_vulkan_upload_op_command
};

GskVulkanImage *
gsk_vulkan_upload_op (GskVulkanRenderPass *render_pass,
                      GdkVulkanContext    *context,
                      GdkTexture          *texture)
{
  GskVulkanUploadOp *self;

  self = (GskVulkanUploadOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_UPLOAD_OP_CLASS);

  self->texture = g_object_ref (texture);
  self->image = gsk_vulkan_image_new_for_upload (context,
                                                 gdk_texture_get_format (texture),
                                                 gdk_texture_get_width (texture),
                                                 gdk_texture_get_height (texture));

  return self->image;
}

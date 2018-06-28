#ifndef __GSK_VULKAN_TEXTURE_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_TEXTURE_PIPELINE_PRIVATE_H__

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanTexturePipelineLayout GskVulkanTexturePipelineLayout;

#define GSK_TYPE_VULKAN_TEXTURE_PIPELINE (gsk_vulkan_texture_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanTexturePipeline, gsk_vulkan_texture_pipeline, GSK, VULKAN_TEXTURE_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline *     gsk_vulkan_texture_pipeline_new                 (GdkVulkanContext         *context,
                                                                         VkPipelineLayout          layout,
                                                                         const char               *shader_name,
                                                                         VkRenderPass              render_pass);

gsize                   gsk_vulkan_texture_pipeline_count_vertex_data   (GskVulkanTexturePipeline *pipeline);
void                    gsk_vulkan_texture_pipeline_collect_vertex_data (GskVulkanTexturePipeline *pipeline,
                                                                         guchar                   *data,
                                                                         const graphene_rect_t    *rect,
                                                                         const graphene_rect_t    *tex_rect);
gsize                   gsk_vulkan_texture_pipeline_draw                (GskVulkanTexturePipeline *pipeline,
                                                                         VkCommandBuffer           command_buffer,
                                                                         gsize                     offset,
                                                                         gsize                     n_commands);

G_END_DECLS

#endif /* __GSK_VULKAN_TEXTURE_PIPELINE_PRIVATE_H__ */

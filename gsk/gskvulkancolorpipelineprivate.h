#ifndef __GSK_VULKAN_COLOR_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_COLOR_PIPELINE_PRIVATE_H__

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanColorPipelineLayout GskVulkanColorPipelineLayout;

#define GSK_TYPE_VULKAN_COLOR_PIPELINE (gsk_vulkan_color_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanColorPipeline, gsk_vulkan_color_pipeline, GSK, VULKAN_COLOR_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline *     gsk_vulkan_color_pipeline_new                   (GskVulkanPipelineLayout *       layout,
                                                                         const char                     *shader_name,
                                                                         VkRenderPass                    render_pass);

gsize                   gsk_vulkan_color_pipeline_count_vertex_data     (GskVulkanColorPipeline         *pipeline);
void                    gsk_vulkan_color_pipeline_collect_vertex_data   (GskVulkanColorPipeline         *pipeline,
                                                                         guchar                         *data,
                                                                         const graphene_rect_t          *rect,
                                                                         const GdkRGBA                  *color);
gsize                   gsk_vulkan_color_pipeline_draw                  (GskVulkanColorPipeline         *pipeline,
                                                                         VkCommandBuffer                 command_buffer,
                                                                         gsize                           offset,
                                                                         gsize                           n_commands);

G_END_DECLS

#endif /* __GSK_VULKAN_COLOR_PIPELINE_PRIVATE_H__ */

#ifndef __GSK_VULKAN_BORDER_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_BORDER_PIPELINE_PRIVATE_H__

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"
#include "gskroundedrect.h"

G_BEGIN_DECLS

typedef struct _GskVulkanBorderPipelineLayout GskVulkanBorderPipelineLayout;

#define GSK_TYPE_VULKAN_BORDER_PIPELINE (gsk_vulkan_border_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanBorderPipeline, gsk_vulkan_border_pipeline, GSK, VULKAN_BORDER_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline *     gsk_vulkan_border_pipeline_new                  (GdkVulkanContext               *context,
                                                                         VkPipelineLayout                layout,
                                                                         const char                     *shader_name,
                                                                         VkRenderPass                    render_pass);

gsize                   gsk_vulkan_border_pipeline_count_vertex_data    (GskVulkanBorderPipeline        *pipeline);
void                    gsk_vulkan_border_pipeline_collect_vertex_data  (GskVulkanBorderPipeline        *pipeline,
                                                                         guchar                         *data,
                                                                         const GskRoundedRect           *rect,
                                                                         const float                     widths[4],
                                                                         const GdkRGBA                   colors[4]);
gsize                   gsk_vulkan_border_pipeline_draw                 (GskVulkanBorderPipeline        *pipeline,
                                                                         VkCommandBuffer                 command_buffer,
                                                                         gsize                           offset,
                                                                         gsize                           n_commands);

G_END_DECLS

#endif /* __GSK_VULKAN_BORDER_PIPELINE_PRIVATE_H__ */

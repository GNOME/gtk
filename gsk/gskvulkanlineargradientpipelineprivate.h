#ifndef __GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_PRIVATE_H__

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"
#include "gskrendernode.h"

G_BEGIN_DECLS

#define GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS 8

typedef struct _GskVulkanLinearGradientPipelineLayout GskVulkanLinearGradientPipelineLayout;

#define GSK_TYPE_VULKAN_LINEAR_GRADIENT_PIPELINE (gsk_vulkan_linear_gradient_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanLinearGradientPipeline, gsk_vulkan_linear_gradient_pipeline, GSK, VULKAN_LINEAR_GRADIENT_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline *     gsk_vulkan_linear_gradient_pipeline_new         (GskVulkanPipelineLayout *       layout,
                                                                         const char                     *shader_name,
                                                                         VkRenderPass                    render_pass);

gsize                   gsk_vulkan_linear_gradient_pipeline_count_vertex_data
                                                                        (GskVulkanLinearGradientPipeline*pipeline);
void                    gsk_vulkan_linear_gradient_pipeline_collect_vertex_data
                                                                        (GskVulkanLinearGradientPipeline*pipeline,
                                                                         guchar                         *data,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *start,
                                                                         const graphene_point_t         *end,
                                                                         gboolean                        repeating,
                                                                         gsize                           n_stops,
                                                                         const GskColorStop             *stops);
gsize                   gsk_vulkan_linear_gradient_pipeline_draw        (GskVulkanLinearGradientPipeline*pipeline,
                                                                         VkCommandBuffer                 command_buffer,
                                                                         gsize                           offset,
                                                                         gsize                           n_commands);

G_END_DECLS

#endif /* __GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_PRIVATE_H__ */

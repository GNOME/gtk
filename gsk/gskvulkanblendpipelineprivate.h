#ifndef __GSK_VULKAN_BLEND_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_BLEND_PIPELINE_PRIVATE_H__

#include <gdk/gdk.h>

#include "gskvulkanpipelineprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanBlendPipelineLayout GskVulkanBlendPipelineLayout;

#define GSK_TYPE_VULKAN_BLEND_PIPELINE (gsk_vulkan_blend_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanBlendPipeline, gsk_vulkan_blend_pipeline, GSK, VULKAN_BLEND_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline *     gsk_vulkan_blend_pipeline_new                   (GskVulkanPipelineLayout *       layout,
                                                                         const char                     *shader_name,
                                                                         VkRenderPass                    render_pass);

G_END_DECLS

#endif /* __GSK_VULKAN_BLEND_PIPELINE_PRIVATE_H__ */

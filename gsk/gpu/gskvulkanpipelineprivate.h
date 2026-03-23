#pragma once

#include "gskvulkandeviceprivate.h"

G_BEGIN_DECLS


void                            gsk_vulkan_pipeline_init_cache          (GskGpuCache                    *cache);
void                            gsk_vulkan_pipeline_finish_cache        (GskGpuCache                    *cache);

GskVulkanPipeline *             gsk_vulkan_pipeline_get                 (GskVulkanDevice                *self,
                                                                         VkPipelineLayout                vk_layout,
                                                                         const GskGpuShaderOpClass      *op_class,
                                                                         GskGpuShaderFlags               flags,
                                                                         GskGpuColorStates               color_states,
                                                                         guint32                         variation,
                                                                         GskGpuBlend                     blend,
                                                                         VkFormat                        vk_format,
                                                                         VkRenderPass                    render_pass);

VkPipeline                      gsk_vulkan_pipeline_get_vk_pipeline     (GskVulkanPipeline              *self);


G_END_DECLS

#ifndef __GSK_VULKAN_RENDER_PASS_PRIVATE_H__
#define __GSK_VULKAN_RENDER_PASS_PRIVATE_H__

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gsk/gskvulkanrenderprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanRenderPass GskVulkanRenderPass;

GskVulkanRenderPass *   gsk_vulkan_render_pass_new                      (GdkVulkanContext       *context);
void                    gsk_vulkan_render_pass_free                     (GskVulkanRenderPass    *self);

void                    gsk_vulkan_render_pass_add_node                 (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         GskRenderNode          *node);

void                    gsk_vulkan_render_pass_upload                   (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         VkCommandBuffer         command_buffer);

gsize                   gsk_vulkan_render_pass_count_vertices           (GskVulkanRenderPass    *self);
gsize                   gsk_vulkan_render_pass_collect_vertices         (GskVulkanRenderPass    *self,
                                                                         GskVulkanVertex        *vertices,
                                                                         gsize                   offset,
                                                                         gsize                   total);

void                    gsk_vulkan_render_pass_update_descriptor_sets   (GskVulkanRenderPass    *self,
                                                                         VkDescriptorSet         descriptor_set,
                                                                         VkSampler               sampler);
void                    gsk_vulkan_render_pass_draw                     (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         VkCommandBuffer         command_buffer);

G_END_DECLS

#endif /* __GSK_VULKAN_RENDER_PASS_PRIVATE_H__ */

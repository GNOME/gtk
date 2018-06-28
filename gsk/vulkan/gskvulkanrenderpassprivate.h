#ifndef __GSK_VULKAN_RENDER_PASS_PRIVATE_H__
#define __GSK_VULKAN_RENDER_PASS_PRIVATE_H__

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gskvulkanbufferprivate.h"
#include "gskvulkanrenderprivate.h"
#include "gsk/gskprivate.h"

G_BEGIN_DECLS


GskVulkanRenderPass *   gsk_vulkan_render_pass_new                      (GdkVulkanContext       *context,
                                                                         GskVulkanImage         *target,
                                                                         int                     scale_factor,
                                                                         graphene_matrix_t      *mv,
                                                                         graphene_rect_t        *viewport,
                                                                         cairo_region_t         *clip,
                                                                         VkSemaphore             signal_semaphore);

void                    gsk_vulkan_render_pass_free                     (GskVulkanRenderPass    *self);

void                    gsk_vulkan_render_pass_add                      (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         GskRenderNode          *node);

void                    gsk_vulkan_render_pass_upload                   (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanUploader      *uploader);
void                    gsk_vulkan_render_pass_reserve_descriptor_sets  (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render);
void                    gsk_vulkan_render_pass_draw                     (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         guint                   layout_count,
                                                                         VkPipelineLayout       *pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);
gsize                   gsk_vulkan_render_pass_get_wait_semaphores      (GskVulkanRenderPass    *self,
                                                                         VkSemaphore           **semaphores);
gsize                   gsk_vulkan_render_pass_get_signal_semaphores    (GskVulkanRenderPass    *self,
                                                                         VkSemaphore           **semaphores);

G_END_DECLS

#endif /* __GSK_VULKAN_RENDER_PASS_PRIVATE_H__ */

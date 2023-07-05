#pragma once

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gskvulkanbufferprivate.h"
#include "gskvulkanrenderprivate.h"
#include "gsk/gskprivate.h"

G_BEGIN_DECLS


GskVulkanRenderPass *   gsk_vulkan_render_pass_new                      (GdkVulkanContext       *context,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanImage         *target,
                                                                         const graphene_vec2_t  *scale,
                                                                         const graphene_rect_t  *viewport,
                                                                         cairo_region_t         *clip,
                                                                         GskRenderNode          *node,
                                                                         gboolean                is_root);

void                    gsk_vulkan_render_pass_free                     (GskVulkanRenderPass    *self);

void                    gsk_vulkan_render_pass_print                    (GskVulkanRenderPass    *self,
                                                                         GString                *string,
                                                                         guint                   indent);

gpointer                gsk_vulkan_render_pass_alloc_op                 (GskVulkanRenderPass    *self,
                                                                         gsize                   size);

void                    gsk_vulkan_render_pass_upload                   (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanUploader      *uploader);
void                    gsk_vulkan_render_pass_reserve_descriptor_sets  (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render);
void                    gsk_vulkan_render_pass_draw                     (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);

G_END_DECLS


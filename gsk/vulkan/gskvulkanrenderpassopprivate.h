#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_render_pass_op                       (GskVulkanRender                *render,
                                                                         GdkVulkanContext               *context,
                                                                         GskVulkanImage                 *image,
                                                                         cairo_region_t                 *clip,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport,
                                                                         GskRenderNode                  *node,
                                                                         VkImageLayout                   initial_layout,
                                                                         VkImageLayout                   final_layout);
GskVulkanImage *        gsk_vulkan_render_pass_op_offscreen             (GskVulkanRender                *render,
                                                                         GdkVulkanContext               *context,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport,
                                                                         GskRenderNode                  *node);

G_END_DECLS


#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS


void                    gsk_vulkan_render_pass_begin_op                 (GskVulkanRender                *render,
                                                                         GskVulkanImage                 *image,
                                                                         const cairo_rectangle_int_t    *area,
                                                                         VkImageLayout                   initial_layout,
                                                                         VkImageLayout                   final_layout);
void                    gsk_vulkan_render_pass_end_op                   (GskVulkanRender                *render,
                                                                         GskVulkanImage                 *image,
                                                                         VkImageLayout                   final_layout);

GskVulkanImage *        gsk_vulkan_render_pass_op_offscreen             (GskVulkanRender                *render,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport,
                                                                         GskRenderNode                  *node);

G_END_DECLS


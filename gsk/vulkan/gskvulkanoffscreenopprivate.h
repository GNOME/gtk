#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

gsize                   gsk_vulkan_offscreen_op_size                    (void) G_GNUC_CONST;

GskVulkanImage *        gsk_vulkan_offscreen_op_init                    (GskVulkanOp                    *op,
                                                                         GdkVulkanContext               *context,
                                                                         GskVulkanRender                *render,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport,
                                                                         VkSemaphore                     signal_semaphore,
                                                                         GskRenderNode                  *node);

G_END_DECLS


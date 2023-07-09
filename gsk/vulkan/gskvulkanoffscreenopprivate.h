#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

GskVulkanImage *        gsk_vulkan_offscreen_op                         (GskVulkanRender                *render,
                                                                         GdkVulkanContext               *context,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport,
                                                                         GskRenderNode                  *node);

G_END_DECLS


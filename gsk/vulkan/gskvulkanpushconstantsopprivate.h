#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

const VkPushConstantRange *
                        gsk_vulkan_push_constants_get_ranges            (void) G_GNUC_PURE;
uint32_t                gsk_vulkan_push_constants_get_range_count       (void) G_GNUC_PURE;

void                    gsk_vulkan_push_constants_op                    (GskVulkanRender                *render,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_matrix_t        *mvp,
                                                                         const GskRoundedRect           *clip);


G_END_DECLS


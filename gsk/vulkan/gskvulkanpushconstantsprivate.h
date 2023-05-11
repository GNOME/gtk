#pragma once

#include <gdk/gdk.h>
#include <graphene.h>
#include "gskvulkanclipprivate.h"

G_BEGIN_DECLS

const VkPushConstantRange *
                        gsk_vulkan_push_constants_get_ranges            (void) G_GNUC_PURE;
uint32_t                gsk_vulkan_push_constants_get_range_count       (void) G_GNUC_PURE;

void                    gsk_vulkan_push_constants_push                  (VkCommandBuffer                 command_buffer,
                                                                         VkPipelineLayout                pipeline_layout,
                                                                         const graphene_matrix_t        *mvp,
                                                                         const GskRoundedRect           *clip);

G_END_DECLS


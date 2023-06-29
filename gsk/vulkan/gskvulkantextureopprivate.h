#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_texture_op                           (GskVulkanRenderPass            *render_pass,
                                                                         const char                     *clip_type,
                                                                         GskVulkanImage                 *image,
                                                                         GskVulkanRenderSampler          sampler,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect);


G_END_DECLS


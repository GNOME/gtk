#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_texture_op                           (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         GskVulkanImage                 *image,
                                                                         GskVulkanRenderSampler          sampler,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect);


G_END_DECLS


#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_blend_mode_op                        (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         const graphene_rect_t          *bounds,
                                                                         const graphene_point_t         *offset,
                                                                         GskBlendMode                    blend_mode,
                                                                         GskVulkanImage                 *top_image,
                                                                         const graphene_rect_t          *top_rect,
                                                                         const graphene_rect_t          *top_tex_rect,
                                                                         GskVulkanImage                 *bottom_image,
                                                                         const graphene_rect_t          *bottom_rect,
                                                                         const graphene_rect_t          *bottom_tex_rect);


G_END_DECLS


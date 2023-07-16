#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_cross_fade_op                        (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         const graphene_rect_t          *bounds,
                                                                         const graphene_point_t         *offset,
                                                                         float                           progress,
                                                                         GskVulkanImage                 *start_image,
                                                                         const graphene_rect_t          *start_rect,
                                                                         const graphene_rect_t          *start_tex_rect,
                                                                         GskVulkanImage                 *end_image,
                                                                         const graphene_rect_t          *end_rect,
                                                                         const graphene_rect_t          *end_tex_rect);


G_END_DECLS


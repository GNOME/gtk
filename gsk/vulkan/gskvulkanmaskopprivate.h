#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_mask_op                              (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         const graphene_point_t         *offset,
                                                                         GskVulkanImage                 *source,
                                                                         const graphene_rect_t          *source_rect,
                                                                         const graphene_rect_t          *source_tex_rect,
                                                                         GskVulkanImage                 *mask,
                                                                         const graphene_rect_t          *mask_rect,
                                                                         const graphene_rect_t          *mask_tex_rect,
                                                                         GskMaskMode                     mask_mode);


G_END_DECLS


#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_linear_gradient_op                   (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_point_t         *start,
                                                                         const graphene_point_t         *end,
                                                                         gboolean                        repeating,
                                                                         const GskColorStop             *stops,
                                                                         gsize                           n_stops);


G_END_DECLS


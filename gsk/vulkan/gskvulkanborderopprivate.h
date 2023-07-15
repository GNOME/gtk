#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_border_op                            (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         const GskRoundedRect           *outline,
                                                                         const graphene_point_t         *offset,
                                                                         const float                     widths[4],
                                                                         const GdkRGBA                   colors[4]);


G_END_DECLS


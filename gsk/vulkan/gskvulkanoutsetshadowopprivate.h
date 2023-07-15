#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_outset_shadow_op                     (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         const GskRoundedRect           *outline,
                                                                         const graphene_point_t         *offset,
                                                                         const GdkRGBA                  *color,
                                                                         const graphene_point_t         *shadow_offset,
                                                                         float                           spread,
                                                                         float                           blur_radius);


G_END_DECLS


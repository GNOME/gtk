#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_fill_op                              (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *rect,
                                                                         GskPath                        *path,
                                                                         GskFillRule                     fill_rule,
                                                                         const GdkRGBA                  *color);


G_END_DECLS


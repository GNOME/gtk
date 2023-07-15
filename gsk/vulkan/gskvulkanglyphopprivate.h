#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_glyph_op                             (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         GskVulkanImage                 *image,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect,
                                                                         const GdkRGBA                  *color);


G_END_DECLS


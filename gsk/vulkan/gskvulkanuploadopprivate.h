#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

GskVulkanImage *        gsk_vulkan_upload_texture_op                    (GskVulkanRender                *render,
                                                                         GdkTexture                     *texture);

GskVulkanImage *        gsk_vulkan_upload_cairo_op                      (GskVulkanRender                *render,
                                                                         GskRenderNode                  *node,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport);

void                    gsk_vulkan_upload_glyph_op                      (GskVulkanRender                *render,
                                                                         GskVulkanImage                 *image,
                                                                         cairo_rectangle_int_t          *area,
                                                                         PangoFont                      *font,
                                                                         PangoGlyphInfo                 *glyph_info,
                                                                         float                           scale);

G_END_DECLS


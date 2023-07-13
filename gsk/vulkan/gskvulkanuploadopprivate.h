#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

GskVulkanImage *        gsk_vulkan_upload_texture_op                    (GskVulkanRender                *render,
                                                                         GdkVulkanContext               *context,
                                                                         GdkTexture                     *texture);

GskVulkanImage *        gsk_vulkan_upload_cairo_op                      (GskVulkanRender                *render,
                                                                         GdkVulkanContext               *context,
                                                                         GskRenderNode                  *node,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport);


G_END_DECLS


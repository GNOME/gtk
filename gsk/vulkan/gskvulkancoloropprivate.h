#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_color_op                             (GskVulkanRenderPass            *render_pass,
                                                                         const char                     *clip_type,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const GdkRGBA                  *color);


G_END_DECLS


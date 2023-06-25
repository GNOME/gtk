#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

gsize                   gsk_vulkan_upload_cairo_op_size                 (void) G_GNUC_CONST;

GskVulkanImage *        gsk_vulkan_upload_cairo_op_init                 (GskVulkanOp                    *op,
                                                                         GdkVulkanContext               *context,
                                                                         GskRenderNode                  *node,
                                                                         const graphene_vec2_t          *scale,
                                                                         const graphene_rect_t          *viewport);



G_END_DECLS


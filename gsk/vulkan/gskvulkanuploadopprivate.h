#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

gsize                   gsk_vulkan_upload_op_size                       (void) G_GNUC_CONST;

GskVulkanImage *        gsk_vulkan_upload_op_init_texture               (GskVulkanOp                    *op,
                                                                         GdkVulkanContext               *context,
                                                                         GdkTexture                     *texture);


G_END_DECLS


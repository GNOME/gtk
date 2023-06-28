#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

GskVulkanImage *        gsk_vulkan_upload_op                            (GskVulkanRenderPass            *render_pass,
                                                                         GdkVulkanContext               *context,
                                                                         GdkTexture                     *texture);


G_END_DECLS


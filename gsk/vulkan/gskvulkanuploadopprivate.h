#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

GskVulkanImage *        gsk_vulkan_upload_op                            (GskVulkanRender                *render,
                                                                         GdkVulkanContext               *context,
                                                                         GdkTexture                     *texture);


G_END_DECLS


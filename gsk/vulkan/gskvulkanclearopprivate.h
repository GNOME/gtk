#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_clear_op                             (GskVulkanRender                *render,
                                                                         const cairo_rectangle_int_t    *rect,
                                                                         const GdkRGBA                  *color);


G_END_DECLS


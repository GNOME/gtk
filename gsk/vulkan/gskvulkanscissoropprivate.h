#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

gsize                   gsk_vulkan_scissor_op_size                      (void) G_GNUC_CONST;

void                    gsk_vulkan_scissor_op_init                      (GskVulkanOp                    *op,
                                                                         const cairo_rectangle_int_t    *rect);


G_END_DECLS


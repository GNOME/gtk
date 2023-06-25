#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

gsize                   gsk_vulkan_color_matrix_op_size                 (void) G_GNUC_CONST;

void                    gsk_vulkan_color_matrix_op_init                 (GskVulkanOp                    *op,
                                                                         GskVulkanPipeline              *pipeline,
                                                                         GskVulkanImage                 *image,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect,
                                                                         const graphene_matrix_t        *color_matrix,
                                                                         const graphene_vec4_t          *color_offset);


G_END_DECLS


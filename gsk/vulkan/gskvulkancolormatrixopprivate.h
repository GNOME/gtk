#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

void                    gsk_vulkan_color_matrix_op                      (GskVulkanRenderPass            *render_pass,
                                                                         const char                     *clip_type,
                                                                         GskVulkanImage                 *image,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect,
                                                                         const graphene_matrix_t        *color_matrix,
                                                                         const graphene_vec4_t          *color_offset);

void                    gsk_vulkan_color_matrix_op_opacity              (GskVulkanRenderPass            *render_pass,
                                                                         const char                     *clip_type,
                                                                         GskVulkanImage                 *image,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect,
                                                                         float                           opacity);


G_END_DECLS


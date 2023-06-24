#pragma once

#include "gskvulkanopprivate.h"

G_BEGIN_DECLS

gsize                   gsk_vulkan_texture_op_size                      (void) G_GNUC_CONST;

void                    gsk_vulkan_texture_op_init                      (GskVulkanOp                    *op,
                                                                         GskVulkanPipeline              *pipeline,
                                                                         GskVulkanImage                 *image,
                                                                         GskVulkanRenderSampler          sampler,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect);


G_END_DECLS


#ifndef __GSK_VULKAN_PUSH_CONSTANTS_PRIVATE_H__
#define __GSK_VULKAN_PUSH_CONSTANTS_PRIVATE_H__

#include <gdk/gdk.h>
#include <graphene.h>
#include "gskvulkanclipprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanPushConstants GskVulkanPushConstants;

struct _GskVulkanPushConstants
{
  graphene_matrix_t mvp;
  GskVulkanClip clip;
};

const VkPushConstantRange *
                        gsk_vulkan_push_constants_get_ranges            (void) G_GNUC_PURE;
uint32_t                gsk_vulkan_push_constants_get_range_count       (void) G_GNUC_PURE;

void                    gsk_vulkan_push_constants_init                  (GskVulkanPushConstants         *constants,
                                                                         const graphene_matrix_t        *mvp,
                                                                         const graphene_rect_t          *viewport);
void                    gsk_vulkan_push_constants_init_copy             (GskVulkanPushConstants         *self,
                                                                         const GskVulkanPushConstants   *src);

gboolean                gsk_vulkan_push_constants_transform             (GskVulkanPushConstants         *self,
                                                                         const GskVulkanPushConstants   *src,
                                                                         GskTransform                   *transform,
                                                                         const graphene_rect_t          *viewport);
gboolean                gsk_vulkan_push_constants_intersect_rect        (GskVulkanPushConstants         *self,
                                                                         const GskVulkanPushConstants   *src,
                                                                         const graphene_rect_t          *rect);
gboolean                gsk_vulkan_push_constants_intersect_rounded     (GskVulkanPushConstants         *self,
                                                                         const GskVulkanPushConstants   *src,
                                                                         const GskRoundedRect           *rect);

void                    gsk_vulkan_push_constants_push                  (const GskVulkanPushConstants   *self,
                                                                         VkCommandBuffer                 command_buffer,
                                                                         VkPipelineLayout                pipeline_layout);

G_END_DECLS

#endif /* __GSK_VULKAN_PUSH_CONSTANTS_PRIVATE_H__ */

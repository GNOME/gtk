#ifndef __GSK_VULKAN_PUSH_CONSTANTS_PRIVATE_H__
#define __GSK_VULKAN_PUSH_CONSTANTS_PRIVATE_H__

#include <gdk/gdk.h>
#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GskVulkanPushConstants GskVulkanPushConstants;
typedef struct _GskVulkanPushConstantsWire GskVulkanPushConstantsWire;

struct _GskVulkanPushConstants
{
  graphene_matrix_t mvp;
};

struct _GskVulkanPushConstantsWire
{
  struct {
    float mvp[16];
  } vertex;
#if 0
  struct {
  } fragment;
#endif
};

const VkPushConstantRange *
                        gst_vulkan_push_constants_get_ranges            (void) G_GNUC_PURE;
uint32_t                gst_vulkan_push_constants_get_range_count       (void) G_GNUC_PURE;

void                    gsk_vulkan_push_constants_init                  (GskVulkanPushConstants         *constants,
                                                                         const graphene_matrix_t        *mvp);
void                    gsk_vulkan_push_constants_init_copy             (GskVulkanPushConstants         *self,
                                                                         const GskVulkanPushConstants   *src);
void                    gsk_vulkan_push_constants_init_transform        (GskVulkanPushConstants         *self,
                                                                         const GskVulkanPushConstants   *src,
                                                                         const graphene_matrix_t        *transform);

void                    gsk_vulkan_push_constants_push_vertex           (const GskVulkanPushConstants   *self,
                                                                         VkCommandBuffer                 command_buffer,
                                                                         VkPipelineLayout                pipeline_layout);

G_END_DECLS

#endif /* __GSK_VULKAN_PUSH_CONSTANTS_PRIVATE_H__ */

#ifndef __GSK_VULKAN_PUSH_CONSTANTS_PRIVATE_H__
#define __GSK_VULKAN_PUSH_CONSTANTS_PRIVATE_H__

#include <gdk/gdk.h>
#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GskVulkanPushConstants GskVulkanPushConstants;

struct _GskVulkanPushConstants
{
  struct {
    float mvp[16];
  } vertex;
  struct {
    float color[4];
  } fragment;
};

const VkPushConstantRange *
                        gst_vulkan_push_constants_get_ranges            (void) G_GNUC_PURE;
uint32_t                gst_vulkan_push_constants_get_range_count       (void) G_GNUC_PURE;

void                    gsk_vulkan_push_constants_init                  (GskVulkanPushConstants         *constants,
                                                                         const graphene_matrix_t        *mvp);
void                    gsk_vulkan_push_constants_init_copy             (GskVulkanPushConstants         *self,
                                                                         const GskVulkanPushConstants   *src);

void                    gsk_vulkan_push_constants_set_mvp               (GskVulkanPushConstants         *self,
                                                                         const graphene_matrix_t        *mvp);
void                    gsk_vulkan_push_constants_multiply_mvp          (GskVulkanPushConstants         *self,
                                                                         const graphene_matrix_t        *transform);
void                    gsk_vulkan_push_constants_set_color             (GskVulkanPushConstants         *self,
                                                                         const GdkRGBA                  *color);

void                    gsk_vulkan_push_constants_push_vertex           (GskVulkanPushConstants         *self,
                                                                         VkCommandBuffer                 command_buffer,
                                                                         VkPipelineLayout                pipeline_layout);
void                    gsk_vulkan_push_constants_push_fragment         (GskVulkanPushConstants         *self,
                                                                         VkCommandBuffer                 command_buffer,
                                                                         VkPipelineLayout                pipeline_layout);

G_END_DECLS

#endif /* __GSK_VULKAN_PUSH_CONSTANTS_PRIVATE_H__ */

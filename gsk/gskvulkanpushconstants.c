#include "config.h"

#include "gskvulkanpushconstantsprivate.h"

#include <math.h>

void
gsk_vulkan_push_constants_init (GskVulkanPushConstants  *constants,
                                const graphene_matrix_t *mvp)
{
  gsk_vulkan_push_constants_set_mvp (constants, mvp);
}

void
gsk_vulkan_push_constants_init_copy (GskVulkanPushConstants       *self,
                                     const GskVulkanPushConstants *src)
{
  *self = *src;
}

void
gsk_vulkan_push_constants_set_mvp (GskVulkanPushConstants  *self,
                                   const graphene_matrix_t *mvp)
{
  graphene_matrix_to_float (mvp, self->vertex.mvp);
}

void
gsk_vulkan_push_constants_multiply_mvp (GskVulkanPushConstants  *self,
                                        const graphene_matrix_t *transform)
{
  graphene_matrix_t old_mvp, new_mvp;

  graphene_matrix_init_from_float (&old_mvp, self->vertex.mvp);
  graphene_matrix_multiply (transform, &old_mvp, &new_mvp);
  gsk_vulkan_push_constants_set_mvp (self, &new_mvp);
}

void
gsk_vulkan_push_constants_push_vertex (GskVulkanPushConstants *self,
                                       VkCommandBuffer         command_buffer,
                                       VkPipelineLayout        pipeline_layout)
{
  vkCmdPushConstants (command_buffer,
                      pipeline_layout,
                      VK_SHADER_STAGE_VERTEX_BIT,
                      G_STRUCT_OFFSET (GskVulkanPushConstants, vertex),
                      sizeof (self->vertex),
                      &self->vertex);
}

#if 0
void
gsk_vulkan_push_constants_push_fragment (GskVulkanPushConstants *self,
                                         VkCommandBuffer         command_buffer,
                                         VkPipelineLayout        pipeline_layout)
{
  vkCmdPushConstants (command_buffer,
                      pipeline_layout,
                      VK_SHADER_STAGE_FRAGMENT_BIT,
                      G_STRUCT_OFFSET (GskVulkanPushConstants, fragment),
                      sizeof (self->fragment),
                      &self->fragment);
}
#endif

uint32_t
gst_vulkan_push_constants_get_range_count (void)
{
  return 2;
}

const VkPushConstantRange *
gst_vulkan_push_constants_get_ranges (void)
{
  static const VkPushConstantRange ranges[2] = {
      {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = G_STRUCT_OFFSET (GskVulkanPushConstants, vertex),
          .size = sizeof (((GskVulkanPushConstants *) 0)->vertex)
#if 0
      },
      {
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = G_STRUCT_OFFSET (GskVulkanPushConstants, fragment),
          .size = sizeof (((GskVulkanPushConstants *) 0)->fragment)
#endif
      }
  };

  return ranges;
}

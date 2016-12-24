#include "config.h"

#include "gskvulkanpushconstantsprivate.h"

#include <math.h>

void
gsk_vulkan_push_constants_init (GskVulkanPushConstants  *constants,
                                const graphene_matrix_t *mvp)
{
  graphene_matrix_init_from_matrix (&constants->mvp, mvp);
}

void
gsk_vulkan_push_constants_init_copy (GskVulkanPushConstants       *self,
                                     const GskVulkanPushConstants *src)
{
  *self = *src;
}

void
gsk_vulkan_push_constants_init_transform (GskVulkanPushConstants       *self,
                                          const GskVulkanPushConstants *src,
                                          const graphene_matrix_t      *transform)

{
  graphene_matrix_multiply (transform, &src->mvp, &self->mvp);
}

static void
gsk_vulkan_push_constants_wire_init (GskVulkanPushConstantsWire   *wire,
                                     const GskVulkanPushConstants *self)
{
  graphene_matrix_to_float (&self->mvp, wire->vertex.mvp);
}

void
gsk_vulkan_push_constants_push_vertex (const GskVulkanPushConstants *self,
                                       VkCommandBuffer               command_buffer,
                                       VkPipelineLayout              pipeline_layout)
{
  GskVulkanPushConstantsWire wire;

  gsk_vulkan_push_constants_wire_init (&wire, self);

  vkCmdPushConstants (command_buffer,
                      pipeline_layout,
                      VK_SHADER_STAGE_VERTEX_BIT,
                      G_STRUCT_OFFSET (GskVulkanPushConstantsWire, vertex),
                      sizeof (wire.vertex),
                      &wire.vertex);
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
          .offset = G_STRUCT_OFFSET (GskVulkanPushConstantsWire, vertex),
          .size = sizeof (((GskVulkanPushConstantsWire *) 0)->vertex)
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

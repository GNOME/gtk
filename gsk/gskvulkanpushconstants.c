#include "config.h"

#include "gskvulkanpushconstantsprivate.h"

#include <math.h>

void
gsk_vulkan_push_constants_init (GskVulkanPushConstants  *constants,
                                const graphene_matrix_t *mvp)
{
  GdkRGBA transparent = { 0, 0, 0, 0 };

  gsk_vulkan_push_constants_set_mvp (constants, mvp);
  gsk_vulkan_push_constants_set_color (constants, &transparent);
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
gsk_vulkan_push_constants_set_color (GskVulkanPushConstants *self,
                                     const GdkRGBA          *color)
{
  self->fragment.color[0] = pow (color->red, 2.2);
  self->fragment.color[1] = pow (color->green, 2.2);
  self->fragment.color[2] = pow (color->blue, 2.2);
  self->fragment.color[3] = color->alpha;
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
      },
      {
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = G_STRUCT_OFFSET (GskVulkanPushConstants, fragment),
          .size = sizeof (((GskVulkanPushConstants *) 0)->fragment)
      }
  };

  return ranges;
}

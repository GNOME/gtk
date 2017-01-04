#include "config.h"

#include "gskvulkanpushconstantsprivate.h"

#include "gskroundedrectprivate.h"

void
gsk_vulkan_push_constants_init (GskVulkanPushConstants  *constants,
                                const graphene_matrix_t *mvp,
                                const graphene_rect_t   *viewport)
{
  graphene_matrix_init_from_matrix (&constants->mvp, mvp);
  gsk_vulkan_clip_init_empty (&constants->clip, viewport);
}

void
gsk_vulkan_push_constants_init_copy (GskVulkanPushConstants       *self,
                                     const GskVulkanPushConstants *src)
{
  *self = *src;
}

gboolean
gsk_vulkan_push_constants_transform (GskVulkanPushConstants       *self,
                                     const GskVulkanPushConstants *src,
                                     const graphene_matrix_t      *transform,
                                     const graphene_rect_t        *viewport)

{
  if (!gsk_vulkan_clip_transform (&self->clip, &src->clip, transform, viewport))
    return FALSE;

  graphene_matrix_multiply (transform, &src->mvp, &self->mvp);
  return TRUE;
}

gboolean
gsk_vulkan_push_constants_intersect_rect (GskVulkanPushConstants       *self,
                                          const GskVulkanPushConstants *src,
                                          const graphene_rect_t        *rect)
{
  if (!gsk_vulkan_clip_intersect_rect (&self->clip, &src->clip, rect))
    return FALSE;

  graphene_matrix_init_from_matrix (&self->mvp, &src->mvp);
  return TRUE;
}

gboolean
gsk_vulkan_push_constants_intersect_rounded (GskVulkanPushConstants       *self,
                                             const GskVulkanPushConstants *src,
                                             const GskRoundedRect         *rect)
{
  if (!gsk_vulkan_clip_intersect_rounded_rect (&self->clip, &src->clip, rect))
    return FALSE;

  graphene_matrix_init_from_matrix (&self->mvp, &src->mvp);
  return TRUE;
}

static void
gsk_vulkan_push_constants_wire_init (GskVulkanPushConstantsWire   *wire,
                                     const GskVulkanPushConstants *self)
{
  graphene_matrix_to_float (&self->mvp, wire->vertex.mvp);
  gsk_rounded_rect_to_float (&self->clip.rect, wire->vertex.clip);
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
  return 1;
}

const VkPushConstantRange *
gst_vulkan_push_constants_get_ranges (void)
{
  static const VkPushConstantRange ranges[1] = {
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

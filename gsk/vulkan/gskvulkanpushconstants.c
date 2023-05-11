#include "config.h"

#include "gskvulkanpushconstantsprivate.h"

#include "gskroundedrectprivate.h"
#include "gsktransform.h"

typedef struct _GskVulkanPushConstantsWire GskVulkanPushConstantsWire;

struct _GskVulkanPushConstantsWire
{
  struct {
    float mvp[16];
    float clip[12];
  } common;
};

/* This is the value we know every conformant GPU must provide.
 * See value for maxPushConstantsSize in table 55 of
 * https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#limits-minmax
 */
G_STATIC_ASSERT (sizeof (GskVulkanPushConstantsWire) <= 128);

static void
gsk_vulkan_push_constants_wire_init (GskVulkanPushConstantsWire *wire,
                                     const graphene_matrix_t    *mvp,
                                     const GskRoundedRect       *clip)
{
  graphene_matrix_to_float (mvp, wire->common.mvp);
  gsk_rounded_rect_to_float (clip, graphene_point_zero (), wire->common.clip);
}

void
gsk_vulkan_push_constants_push (VkCommandBuffer          command_buffer,
                                VkPipelineLayout         pipeline_layout,
                                const graphene_matrix_t *mvp,
                                const GskRoundedRect    *clip)
{
  GskVulkanPushConstantsWire wire;

  gsk_vulkan_push_constants_wire_init (&wire, mvp, clip);

  vkCmdPushConstants (command_buffer,
                      pipeline_layout,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      G_STRUCT_OFFSET (GskVulkanPushConstantsWire, common),
                      sizeof (wire.common),
                      &wire.common);
}

uint32_t
gsk_vulkan_push_constants_get_range_count (void)
{
  return 1;
}

const VkPushConstantRange *
gsk_vulkan_push_constants_get_ranges (void)
{
  static const VkPushConstantRange ranges[1] = {
      {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = G_STRUCT_OFFSET (GskVulkanPushConstantsWire, common),
          .size = sizeof (((GskVulkanPushConstantsWire *) 0)->common)
      }
  };

  return ranges;
}

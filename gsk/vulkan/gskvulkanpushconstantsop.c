#include "config.h"

#include "gskvulkanpushconstantsopprivate.h"

#include "gskroundedrectprivate.h"
#include "gskvulkanprivate.h"

typedef struct _GskVulkanPushConstantsOp GskVulkanPushConstantsOp;
typedef struct _GskVulkanPushConstantsInstance GskVulkanPushConstantsInstance;

struct _GskVulkanPushConstantsInstance
{
  float mvp[16];
  float clip[12];
  float scale[2];
};

struct _GskVulkanPushConstantsOp
{
  GskVulkanOp op;

  GskVulkanPushConstantsInstance instance;
};

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
          .offset = 0,
          .size = sizeof (GskVulkanPushConstantsInstance)
      }
  };

  return ranges;
}

static void
gsk_vulkan_push_constants_op_finish (GskVulkanOp *op)
{
}

static void
gsk_vulkan_push_constants_op_print (GskVulkanOp *op,
                                    GString     *string,
                                    guint        indent)
{
  print_indent (string, indent);
  g_string_append_printf (string, "push-constants ");
  print_newline (string);
}

static gsize
gsk_vulkan_push_constants_op_count_vertex_data (GskVulkanOp *op,
                                                gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_push_constants_op_collect_vertex_data (GskVulkanOp *op,
                                                  guchar      *data)
{
}

static void
gsk_vulkan_push_constants_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                      GskVulkanRender *render)
{
}

static GskVulkanOp *
gsk_vulkan_push_constants_op_command (GskVulkanOp      *op,
                                      GskVulkanRender  *render,
                                      VkRenderPass      render_pass,
                                      VkCommandBuffer   command_buffer)
{
  GskVulkanPushConstantsOp *self = (GskVulkanPushConstantsOp *) op;

  vkCmdPushConstants (command_buffer,
                      gsk_vulkan_render_get_pipeline_layout (render),
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      0,
                      sizeof (self->instance),
                      &self->instance);

  return op->next;
}

static const GskVulkanOpClass GSK_VULKAN_PUSH_CONSTANTS_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanPushConstantsOp),
  GSK_VULKAN_STAGE_COMMAND,
  gsk_vulkan_push_constants_op_finish,
  gsk_vulkan_push_constants_op_print,
  gsk_vulkan_push_constants_op_count_vertex_data,
  gsk_vulkan_push_constants_op_collect_vertex_data,
  gsk_vulkan_push_constants_op_reserve_descriptor_sets,
  gsk_vulkan_push_constants_op_command
};

void
gsk_vulkan_push_constants_op (GskVulkanRender         *render,
                              const graphene_vec2_t   *scale,
                              const graphene_matrix_t *mvp,
                              const GskRoundedRect    *clip)
{
  GskVulkanPushConstantsOp *self;

  self = (GskVulkanPushConstantsOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_PUSH_CONSTANTS_OP_CLASS);

  graphene_matrix_to_float (mvp, self->instance.mvp);
  gsk_rounded_rect_to_float (clip, graphene_point_zero (), self->instance.clip);
  graphene_vec2_to_float (scale, self->instance.scale);
}

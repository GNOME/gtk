#pragma once

#include <gdk/gdk.h>

#include "gskvulkanrenderpassprivate.h"

G_BEGIN_DECLS

typedef enum
{
  GSK_VULKAN_STAGE_UPLOAD,
  GSK_VULKAN_STAGE_COMMAND,
  /* magic ones */
  GSK_VULKAN_STAGE_BEGIN_PASS,
  GSK_VULKAN_STAGE_END_PASS
} GskVulkanStage;

struct _GskVulkanOp
{
  const GskVulkanOpClass *op_class;

  const /* interned */ char *clip_type;
  GskVulkanOp *next;
  gsize vertex_offset;
};

struct _GskVulkanOpClass
{
  gsize                 size;
  GskVulkanStage        stage;

  const char *          shader_name;
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state;

  void                  (* finish)                                      (GskVulkanOp            *op);

  void                  (* print)                                       (GskVulkanOp            *op,
                                                                         GString                *string,
                                                                         guint                   indent);

  gsize                 (* count_vertex_data)                           (GskVulkanOp            *op,
                                                                         gsize                   n_bytes);
  void                  (* collect_vertex_data)                         (GskVulkanOp            *op,
                                                                         guchar                 *data);
  void                  (* reserve_descriptor_sets)                     (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render);
  GskVulkanOp *         (* command)                                     (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);
};

/* ensures alignment of ops to multipes of 16 bytes - and that makes graphene happy */
#define GSK_VULKAN_OP_SIZE(struct_name) ((sizeof(struct_name) + 15) & ~15)

GskVulkanOp *           gsk_vulkan_op_alloc                             (GskVulkanRender        *render,
                                                                         const GskVulkanOpClass *op_class);
void                    gsk_vulkan_op_finish                            (GskVulkanOp            *op);

void                    gsk_vulkan_op_print                             (GskVulkanOp            *op,
                                                                         GString                *string,
                                                                         guint                   indent);

gsize                   gsk_vulkan_op_count_vertex_data                 (GskVulkanOp            *op,
                                                                         gsize                   n_bytes);
void                    gsk_vulkan_op_collect_vertex_data               (GskVulkanOp            *op,
                                                                         guchar                 *data);
void                    gsk_vulkan_op_reserve_descriptor_sets           (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render);
GskVulkanOp *           gsk_vulkan_op_command                           (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);

gsize                   gsk_vulkan_op_draw_count_vertex_data            (GskVulkanOp            *op,
                                                                         gsize                   n_bytes);
GskVulkanOp *           gsk_vulkan_op_draw_command_n                    (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer,
                                                                         gsize                   instance_scale);
GskVulkanOp *           gsk_vulkan_op_draw_command                      (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);

G_END_DECLS


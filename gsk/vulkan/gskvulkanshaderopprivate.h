#pragma once

#include "gskvulkanopprivate.h"

#include "gskvulkanclipprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanShaderOp GskVulkanShaderOp;
typedef struct _GskVulkanShaderOpClass GskVulkanShaderOpClass;

struct _GskVulkanShaderOp
{
  GskVulkanOp parent_op;

  GskVulkanShaderClip clip;
  gsize vertex_offset;
  GskVulkanImage *images[2];
};

struct _GskVulkanShaderOpClass
{
  GskVulkanOpClass      parent_class;

  const char *          shader_name;
  gsize                 n_images;
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state;
};

GskVulkanShaderOp *     gsk_vulkan_shader_op_alloc                      (GskVulkanRender        *render,
                                                                         const GskVulkanShaderOpClass *op_class,
                                                                         GskVulkanShaderClip     clip,
                                                                         GskVulkanImage        **images);

void                    gsk_vulkan_shader_op_finish                     (GskVulkanOp            *op);
gsize                   gsk_vulkan_shader_op_count_vertex_data          (GskVulkanOp            *op,
                                                                         gsize                   n_bytes);
GskVulkanOp *           gsk_vulkan_shader_op_command_n                  (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render,
                                                                         VkRenderPass            render_pass,
                                                                         VkCommandBuffer         command_buffer,
                                                                         gsize                   instance_scale);
GskVulkanOp *           gsk_vulkan_shader_op_command                    (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render,
                                                                         VkRenderPass            render_pass,
                                                                         VkCommandBuffer         command_buffer);

G_END_DECLS


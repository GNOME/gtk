#pragma once

#include <gdk/gdk.h>

#include "gskvulkanrenderpassprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanOp GskVulkanOp;

struct _GskVulkanOp
{
  const GskVulkanOpClass *op_class;

  const /* interned */ char *clip_type;
  GskVulkanOp *next;
};

struct _GskVulkanOpClass
{
  gsize                 size;

  const char *          shader_name;
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state;

  void                  (* finish)                                      (GskVulkanOp            *op);

  void                  (* print)                                       (GskVulkanOp            *op,
                                                                         GString                *string,
                                                                         guint                   indent);

  void                  (* upload)                                      (GskVulkanOp            *op,
                                                                         GskVulkanRenderPass    *pass,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanUploader      *uploader);
  gsize                 (* count_vertex_data)                           (GskVulkanOp            *op,
                                                                         gsize                   n_bytes);
  void                  (* collect_vertex_data)                         (GskVulkanOp            *op,
                                                                         GskVulkanRenderPass    *pass,
                                                                         GskVulkanRender        *render,
                                                                         guchar                 *data);
  void                  (* reserve_descriptor_sets)                     (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render);
  void                  (* command)                                     (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);
};

/* ensures alignment of ops to multipes of 16 bytes - and that makes graphene happy */
#define GSK_VULKAN_OP_SIZE(struct_name) ((sizeof(struct_name) + 15) & ~15)

GskVulkanOp *           gsk_vulkan_op_alloc                             (GskVulkanRenderPass    *render_pass,
                                                                         const GskVulkanOpClass *op_class);
void                    gsk_vulkan_op_finish                            (GskVulkanOp            *op);

void                    gsk_vulkan_op_print                             (GskVulkanOp            *op,
                                                                         GString                *string,
                                                                         guint                   indent);

void                    gsk_vulkan_op_upload                            (GskVulkanOp            *op,
                                                                         GskVulkanRenderPass    *pass,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanUploader      *uploader);
gsize                   gsk_vulkan_op_count_vertex_data                 (GskVulkanOp            *op,
                                                                         gsize                   n_bytes);
void                    gsk_vulkan_op_collect_vertex_data               (GskVulkanOp            *op,
                                                                         GskVulkanRenderPass    *pass,
                                                                         GskVulkanRender        *render,
                                                                         guchar                 *data);
void                    gsk_vulkan_op_reserve_descriptor_sets           (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render);
void                    gsk_vulkan_op_command                           (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);

G_END_DECLS


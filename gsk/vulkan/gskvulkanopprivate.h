#pragma once

#include <gdk/gdk.h>

#include "gskvulkanrenderpassprivate.h"

G_BEGIN_DECLS

typedef enum
{
  GSK_VULKAN_STAGE_UPLOAD,
  GSK_VULKAN_STAGE_COMMAND,
  GSK_VULKAN_STAGE_SHADER,
  /* magic ones */
  GSK_VULKAN_STAGE_BEGIN_PASS,
  GSK_VULKAN_STAGE_END_PASS
} GskVulkanStage;

struct _GskVulkanOp
{
  const GskVulkanOpClass *op_class;

  GskVulkanOp *next;
};

struct _GskVulkanOpClass
{
  gsize                 size;
  GskVulkanStage        stage;

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
                                                                         VkRenderPass            render_pass,
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
                                                                         VkRenderPass            render_pass,
                                                                         VkCommandBuffer         command_buffer);

G_END_DECLS


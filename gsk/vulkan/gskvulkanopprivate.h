#pragma once

#include <gdk/gdk.h>

#include "gskvulkanrenderpassprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanOp GskVulkanOp;
typedef struct _GskVulkanOpClass GskVulkanOpClass;

struct _GskVulkanOp
{
  const GskVulkanOpClass *op_class;
};

struct _GskVulkanOpClass
{
  gsize                 size;

  void                  (* finish)                                      (GskVulkanOp            *op);

  void                  (* upload)                                      (GskVulkanOp            *op,
                                                                         GskVulkanRenderPass    *pass,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanUploader      *uploader,
                                                                         const graphene_rect_t  *clip,
                                                                         const graphene_vec2_t  *scale);
  gsize                 (* count_vertex_data)                           (GskVulkanOp            *op,
                                                                         gsize                   n_bytes);
  void                  (* collect_vertex_data)                         (GskVulkanOp            *op,
                                                                         GskVulkanRenderPass    *pass,
                                                                         GskVulkanRender        *render,
                                                                         guchar                 *data);
  void                  (* reserve_descriptor_sets)                     (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render);
  GskVulkanPipeline *   (* get_pipeline)                                (GskVulkanOp            *op);
  void                  (* command)                                     (GskVulkanOp            *op,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);
};

void                    gsk_vulkan_op_finish                            (GskVulkanOp            *op);

void                    gsk_vulkan_op_upload                            (GskVulkanOp            *op,
                                                                         GskVulkanRenderPass    *pass,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanUploader      *uploader,
                                                                         const graphene_rect_t  *clip,
                                                                         const graphene_vec2_t  *scale);
gsize                   gsk_vulkan_op_count_vertex_data                 (GskVulkanOp            *op,
                                                                         gsize                   n_bytes);
void                    gsk_vulkan_op_collect_vertex_data               (GskVulkanOp            *op,
                                                                         GskVulkanRenderPass    *pass,
                                                                         GskVulkanRender        *render,
                                                                         guchar                 *data);
void                    gsk_vulkan_op_reserve_descriptor_sets           (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render);
GskVulkanPipeline *     gsk_vulkan_op_get_pipeline                      (GskVulkanOp            *op);
void                    gsk_vulkan_op_command                           (GskVulkanOp            *op,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);

G_END_DECLS


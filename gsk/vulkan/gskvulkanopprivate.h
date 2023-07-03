#pragma once

#include <gdk/gdk.h>

#include "gskvulkanrenderpassprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanOp GskVulkanOp;
typedef struct _GskVulkanOpClass GskVulkanOpClass;

struct _GskVulkanOp
{
  const GskVulkanOpClass *op_class;

  const /* interned */ char *clip_type;
};

struct _GskVulkanOpClass
{
  gsize                 size;

  const char *          shader_name;
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state;

  void                  (* finish)                                      (GskVulkanOp            *op);

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
  VkPipeline            (* get_pipeline)                                (GskVulkanOp            *op);
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
VkPipeline              gsk_vulkan_op_get_pipeline                      (GskVulkanOp            *op);
void                    gsk_vulkan_op_command                           (GskVulkanOp            *op,
                                                                         GskVulkanRender        *render,
                                                                         VkPipelineLayout        pipeline_layout,
                                                                         VkCommandBuffer         command_buffer);

static inline void
gsk_vulkan_normalize_tex_coords (graphene_rect_t       *tex_coords,
                                 const graphene_rect_t *rect,
                                 const graphene_rect_t *tex)
{
  graphene_rect_init (tex_coords,
                      (rect->origin.x - tex->origin.x) / tex->size.width,
                      (rect->origin.y - tex->origin.y) / tex->size.height,
                      rect->size.width / tex->size.width,
                      rect->size.height / tex->size.height);
}

static inline void
gsk_vulkan_rect_to_float (const graphene_rect_t *rect,
                          float                  values[4])
{
  values[0] = rect->origin.x;
  values[1] = rect->origin.y;
  values[2] = rect->size.width;
  values[3] = rect->size.height;
}

static inline void
gsk_vulkan_rgba_to_float (const GdkRGBA *rgba,
                          float          values[4])
{
  values[0] = rgba->red;
  values[1] = rgba->green;
  values[2] = rgba->blue;
  values[3] = rgba->alpha;
}

static inline void
gsk_vulkan_point_to_float (const graphene_point_t *point,
                           float                   values[2])
{
  values[0] = point->x;
  values[1] = point->y;
}

G_END_DECLS


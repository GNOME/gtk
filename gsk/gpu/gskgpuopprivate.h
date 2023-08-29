#pragma once

#include <gdk/gdk.h>

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

typedef enum
{
  GSK_GPU_STAGE_UPLOAD,
  GSK_GPU_STAGE_PASS,
  GSK_GPU_STAGE_COMMAND,
  GSK_GPU_STAGE_SHADER,
  /* magic ones */
  GSK_GPU_STAGE_BEGIN_PASS,
  GSK_GPU_STAGE_END_PASS
} GskGpuStage;

struct _GskGpuOp
{
  const GskGpuOpClass *op_class;

  GskGpuOp *next;
};

struct _GskGpuOpClass
{
  gsize                 size;
  GskGpuStage           stage;

  void                  (* finish)                                      (GskGpuOp               *op);

  void                  (* print)                                       (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GString                *string,
                                                                         guint                   indent);

#ifdef GDK_RENDERING_VULKAN
  GskGpuOp *            (* vk_command)                                  (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         VkRenderPass            render_pass,
                                                                         VkFormat                format,
                                                                         VkCommandBuffer         command_buffer);
#endif
  GskGpuOp *            (* gl_command)                                  (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         gsize                   flip_y);
};

/* ensures alignment of ops to multipes of 16 bytes - and that makes graphene happy */
#define GSK_GPU_OP_SIZE(struct_name) ((sizeof(struct_name) + 15) & ~15)

GskGpuOp *              gsk_gpu_op_alloc                                (GskGpuFrame            *frame,
                                                                         const GskGpuOpClass    *op_class);
void                    gsk_gpu_op_finish                               (GskGpuOp               *op);

void                    gsk_gpu_op_print                                (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GString                *string,
                                                                         guint                   indent);

#ifdef GDK_RENDERING_VULKAN
GskGpuOp *              gsk_gpu_op_vk_command                           (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         VkRenderPass            render_pass,
                                                                         VkFormat                format,
                                                                         VkCommandBuffer         command_buffer);
#endif
GskGpuOp *              gsk_gpu_op_gl_command                           (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         gsize                   flip_y);

G_END_DECLS


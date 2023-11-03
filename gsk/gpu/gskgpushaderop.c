#include "config.h"

#include "gskgpushaderopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgldescriptorsprivate.h"
#include "gskgldeviceprivate.h"
#include "gskglframeprivate.h"
#include "gskglimageprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkandescriptorsprivate.h"
#include "gskvulkandeviceprivate.h"
#endif

void
gsk_gpu_shader_op_finish (GskGpuOp *op)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;

  g_clear_object (&self->desc);
}


#ifdef GDK_RENDERING_VULKAN
GskGpuOp *
gsk_gpu_shader_op_vk_command_n (GskGpuOp        *op,
                                GskGpuFrame     *frame,
                                VkRenderPass     render_pass,
                                VkFormat         format,
                                VkCommandBuffer  command_buffer,
                                gsize            instance_scale)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;
  GskGpuShaderOpClass *shader_op_class = (GskGpuShaderOpClass *) op->op_class;
  GskGpuOp *next;
  gsize i;

  i = 1;
  if (gsk_vulkan_device_has_feature (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                     GDK_VULKAN_FEATURE_NONUNIFORM_INDEXING))
    {
      for (next = op->next; next && i < 10 * 1000; next = next->next)
        {
          GskGpuShaderOp *next_shader = (GskGpuShaderOp *) next;
      
          if (next->op_class != op->op_class ||
              next_shader->vertex_offset != self->vertex_offset + i * shader_op_class->vertex_size)
            break;

          i++;
        }
    }

  vkCmdBindPipeline (command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     gsk_vulkan_device_get_vk_pipeline (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                        gsk_vulkan_frame_get_pipeline_layout (GSK_VULKAN_FRAME (frame)),
                                                        shader_op_class,
                                                        self->clip,
                                                        format,
                                                        render_pass));

  vkCmdDraw (command_buffer,
             6 * instance_scale, i,
             0, self->vertex_offset / shader_op_class->vertex_size);

  return next;
}

GskGpuOp *
gsk_gpu_shader_op_vk_command (GskGpuOp        *op,
                              GskGpuFrame     *frame,
                              VkRenderPass     render_pass,
                              VkFormat         format,
                              VkCommandBuffer  command_buffer)
{
  return gsk_gpu_shader_op_vk_command_n (op, frame, render_pass, format, command_buffer, 1);
}
#endif

GskGpuOp *
gsk_gpu_shader_op_gl_command_n (GskGpuOp    *op,
                                GskGpuFrame *frame,
                                gsize        flip_y,
                                gsize        instance_scale)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;
  GskGpuShaderOpClass *shader_op_class = (GskGpuShaderOpClass *) op->op_class;
  GskGLDescriptors *desc;
  GskGpuOp *next;
  gsize i;

  desc = GSK_GL_DESCRIPTORS (self->desc);
  gsk_gl_frame_use_program (GSK_GL_FRAME (frame),
                            shader_op_class,
                            self->clip,
                            desc ? gsk_gl_descriptors_get_n_external (desc) : 0);

  if (desc)
    gsk_gl_descriptors_use (GSK_GL_DESCRIPTORS (desc));

  i = 1;
  for (next = op->next; next && i < 10 * 1000; next = next->next)
    {
      GskGpuShaderOp *next_shader = (GskGpuShaderOp *) next;

      if (next->op_class != op->op_class ||
          next_shader->desc != self->desc ||
          next_shader->vertex_offset != self->vertex_offset + i * shader_op_class->vertex_size)
        break;

      i++;
    }

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_GL_BASE_INSTANCE))
    {
      glDrawArraysInstancedBaseInstance (GL_TRIANGLES,
                                         0,
                                         6 * instance_scale,
                                         i,
                                         self->vertex_offset / shader_op_class->vertex_size);
    }
  else
    {
      shader_op_class->setup_vao (self->vertex_offset);

      glDrawArraysInstanced (GL_TRIANGLES,
                             0,
                             6 * instance_scale,
                             i);
    }

  return next;
}

GskGpuOp *
gsk_gpu_shader_op_gl_command (GskGpuOp    *op,
                              GskGpuFrame *frame,
                              gsize        flip_y)
{
  return gsk_gpu_shader_op_gl_command_n (op, frame, flip_y, 1);
}

GskGpuShaderOp *
gsk_gpu_shader_op_alloc (GskGpuFrame               *frame,
                         const GskGpuShaderOpClass *op_class,
                         GskGpuShaderClip           clip,
                         GskGpuDescriptors         *desc,
                         gpointer                   out_vertex_data)
{
  GskGpuShaderOp *self;

  self = (GskGpuShaderOp *) gsk_gpu_op_alloc (frame, &op_class->parent_class);

  self->clip = clip;
  if (desc)
    self->desc = g_object_ref (desc);
  else
    self->desc = NULL;
  self->vertex_offset = gsk_gpu_frame_reserve_vertex_data (frame, op_class->vertex_size);

  *((gpointer *) out_vertex_data) = gsk_gpu_frame_get_vertex_data (frame, self->vertex_offset);

  return self;
}


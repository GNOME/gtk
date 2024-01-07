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

/* maximum number of ops to merge into one call
 * If this number is too high, the command may take too long
 * causing the driver to kill us.
 */
#define MAX_MERGE_OPS (10 * 1000)

void
gsk_gpu_shader_op_finish (GskGpuOp *op)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;

  g_clear_object (&self->desc);
}

#ifdef GDK_RENDERING_VULKAN
GskGpuOp *
gsk_gpu_shader_op_vk_command_n (GskGpuOp              *op,
                                GskGpuFrame           *frame,
                                GskVulkanCommandState *state,
                                gsize                  instance_scale)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;
  GskGpuShaderOpClass *shader_op_class = (GskGpuShaderOpClass *) op->op_class;
  GskVulkanDescriptors *desc;
  GskGpuOp *next;
  gsize i, n;

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_MERGE) &&
      gsk_vulkan_device_has_feature (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                     GDK_VULKAN_FEATURE_NONUNIFORM_INDEXING))
    n = MAX_MERGE_OPS;
  else
    n = 1;
  i = 1;
  desc = GSK_VULKAN_DESCRIPTORS (self->desc);
  if (desc && state->desc != desc)
    {
      gsk_vulkan_descriptors_bind (desc, state->desc, state->vk_command_buffer);
      state->desc = desc;
    }

  for (next = op->next; next && i < n; next = next->next)
    {
      GskGpuShaderOp *next_shader = (GskGpuShaderOp *) next;
  
      if (next->op_class != op->op_class ||
          next_shader->desc != self->desc ||
          next_shader->variation != self->variation ||
          next_shader->clip != self->clip ||
          next_shader->vertex_offset != self->vertex_offset + i * shader_op_class->vertex_size)
        break;

      i++;
    }

  vkCmdBindPipeline (state->vk_command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     gsk_vulkan_device_get_vk_pipeline (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                        gsk_vulkan_descriptors_get_pipeline_layout (state->desc),
                                                        shader_op_class,
                                                        self->variation,
                                                        self->clip,
                                                        state->blend,
                                                        state->vk_format,
                                                        state->vk_render_pass));

  vkCmdDraw (state->vk_command_buffer,
             6 * instance_scale, i,
             0, self->vertex_offset / shader_op_class->vertex_size);

  return next;
}

GskGpuOp *
gsk_gpu_shader_op_vk_command (GskGpuOp              *op,
                              GskGpuFrame           *frame,
                              GskVulkanCommandState *state)
{
  return gsk_gpu_shader_op_vk_command_n (op, frame, state, 1);
}
#endif

GskGpuOp *
gsk_gpu_shader_op_gl_command_n (GskGpuOp          *op,
                                GskGpuFrame       *frame,
                                GskGLCommandState *state,
                                gsize              instance_scale)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;
  GskGpuShaderOpClass *shader_op_class = (GskGpuShaderOpClass *) op->op_class;
  GskGLDescriptors *desc;
  GskGpuOp *next;
  gsize i, n, n_external;

  desc = GSK_GL_DESCRIPTORS (self->desc);
  if (desc)
    n_external = gsk_gl_descriptors_get_n_external (desc);
  else
    n_external = 0;

  if (state->current_program.op_class != op->op_class ||
      state->current_program.variation != self->variation ||
      state->current_program.clip != self->clip ||
      state->current_program.n_external != n_external)
    {
      state->current_program.op_class = op->op_class;
      state->current_program.variation = self->variation;
      state->current_program.clip = self->clip;
      state->current_program.n_external = n_external;
      gsk_gl_frame_use_program (GSK_GL_FRAME (frame),
                                shader_op_class,
                                self->variation,
                                self->clip,
                                n_external);
    }

  if (desc != state->desc && desc)
    {
      gsk_gl_descriptors_use (desc);
      state->desc = desc;
    }

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_MERGE))
    n = MAX_MERGE_OPS;
  else
    n = 1;
  i = 1;
  for (next = op->next; next && i < n; next = next->next)
    {
      GskGpuShaderOp *next_shader = (GskGpuShaderOp *) next;

      if (next->op_class != op->op_class ||
          next_shader->desc != self->desc ||
          next_shader->variation != self->variation ||
          next_shader->clip != self->clip ||
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
gsk_gpu_shader_op_gl_command (GskGpuOp          *op,
                              GskGpuFrame       *frame,
                              GskGLCommandState *state)
{
  return gsk_gpu_shader_op_gl_command_n (op, frame, state, 1);
}

GskGpuShaderOp *
gsk_gpu_shader_op_alloc (GskGpuFrame               *frame,
                         const GskGpuShaderOpClass *op_class,
                         guint32                    variation,
                         GskGpuShaderClip           clip,
                         GskGpuDescriptors         *desc,
                         gpointer                   out_vertex_data)
{
  GskGpuShaderOp *self;

  self = (GskGpuShaderOp *) gsk_gpu_op_alloc (frame, &op_class->parent_class);

  self->variation = variation;
  self->clip = clip;
  if (desc)
    self->desc = g_object_ref (desc);
  else
    self->desc = NULL;
  self->vertex_offset = gsk_gpu_frame_reserve_vertex_data (frame, op_class->vertex_size);

  *((gpointer *) out_vertex_data) = gsk_gpu_frame_get_vertex_data (frame, self->vertex_offset);

  return self;
}


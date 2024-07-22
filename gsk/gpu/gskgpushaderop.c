#include "config.h"

#include "gskgpushaderopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskgpushaderflagsprivate.h"
#include "gskgldeviceprivate.h"
#include "gskglframeprivate.h"
#include "gskglimageprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkandeviceprivate.h"
#include "gskvulkanimageprivate.h"
#endif

#include "gdkglcontextprivate.h"

/* maximum number of ops to merge into one call
 * If this number is too high, the command may take too long
 * causing the driver to kill us.
 */
#define MAX_MERGE_OPS (10 * 1000)

void
gsk_gpu_shader_op_finish (GskGpuOp *op)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;

  g_clear_object (&self->images[0]);
  g_clear_object (&self->images[1]);
}

void
gsk_gpu_shader_op_print (GskGpuOp    *op,
                         GskGpuFrame *frame,
                         GString     *string,
                         guint        indent)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;
  const GskGpuShaderOpClass *shader_class = (const GskGpuShaderOpClass *) op->op_class;
  const char *shader_name;
  guchar *instance;
  gsize i;

  if (g_str_has_prefix (shader_class->shader_name, "gskgpu"))
    shader_name = shader_class->shader_name + 6;
  else
    shader_name = shader_class->shader_name;

  instance = gsk_gpu_frame_get_vertex_data (frame, self->vertex_offset);

  for (i = 0; i < self->n_ops; i++)
    {
      gsk_gpu_print_op (string, indent, shader_name);
      gsk_gpu_print_shader_flags (string, self->flags);
      gsk_gpu_print_color_states (string, self->color_states);
      shader_class->print_instance (self,
                                    instance + i * shader_class->vertex_size,
                                    string);
      gsk_gpu_print_newline (string);
    }
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
  GskGpuOp *next;
  VkPipelineLayout vk_pipeline_layout;
  gsize i, n_ops, max_ops_per_draw;

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_MERGE))
    max_ops_per_draw = MAX_MERGE_OPS;
  else
    max_ops_per_draw = 1;

  n_ops = self->n_ops;
  for (next = op->next; next; next = next->next)
    {
      GskGpuShaderOp *next_shader = (GskGpuShaderOp *) next;
  
      if (next->op_class != op->op_class ||
          next_shader->flags != self->flags ||
          next_shader->color_states != self->color_states ||
          next_shader->variation != self->variation ||
          next_shader->vertex_offset != self->vertex_offset + n_ops * shader_op_class->vertex_size ||
          (shader_op_class->n_textures > 0 && (next_shader->images[0] != self->images[0] || next_shader->samplers[0] != self->samplers[0])) ||
          (shader_op_class->n_textures > 1 && (next_shader->images[1] != self->images[1] || next_shader->samplers[1] != self->samplers[1])))
        break;

      n_ops += next_shader->n_ops;
    }

  vk_pipeline_layout = gsk_vulkan_device_get_vk_pipeline_layout (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                                 shader_op_class->n_textures > 0 ? gsk_vulkan_image_get_ycbcr (GSK_VULKAN_IMAGE (self->images[0])) : NULL,
                                                                 shader_op_class->n_textures > 1 ? gsk_vulkan_image_get_ycbcr (GSK_VULKAN_IMAGE (self->images[1])) : NULL);

  for (i = 0; i < shader_op_class->n_textures; i++)
    {
      if (state->current_images[i] != self->images[i] ||
          state->current_samplers[i] != self->samplers[i])
        {
          vkCmdBindDescriptorSets (state->vk_command_buffer,
                                   VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   vk_pipeline_layout,
                                   i,
                                   1,
                                   (VkDescriptorSet[1]) {
                                       gsk_vulkan_image_get_vk_descriptor_set (GSK_VULKAN_IMAGE (self->images[i]), self->samplers[i]),
                                   },
                                   0,
                                   NULL);
          state->current_images[i] = self->images[i];
          state->current_samplers[i] = self->samplers[i];
        }
    }
                               
  vkCmdBindPipeline (state->vk_command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     gsk_vulkan_device_get_vk_pipeline (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                        vk_pipeline_layout,
                                                        shader_op_class,
                                                        self->flags,
                                                        self->color_states,
                                                        self->variation,
                                                        state->blend,
                                                        state->vk_format,
                                                        state->vk_render_pass));

  for (i = 0; i < n_ops; i += max_ops_per_draw)
    {
      vkCmdDraw (state->vk_command_buffer,
                 6 * instance_scale, MIN (max_ops_per_draw, n_ops - i),
                 0, self->vertex_offset / shader_op_class->vertex_size + i);
    }
 
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
  GskGpuOp *next;
  gsize i, n_ops, max_ops_per_draw;

  if (state->current_program.op_class != op->op_class ||
      state->current_program.color_states != self->color_states ||
      state->current_program.variation != self->variation ||
      state->current_program.flags != self->flags)
    {
      state->current_program.op_class = op->op_class;
      state->current_program.flags = self->flags;
      state->current_program.color_states = self->color_states;
      state->current_program.variation = self->variation;
      gsk_gl_frame_use_program (GSK_GL_FRAME (frame),
                                shader_op_class,
                                self->flags,
                                self->color_states,
                                self->variation);
    }

  for (i = 0; i < shader_op_class->n_textures; i++)
    {
      if (state->current_images[i] != self->images[i])
        {
          glActiveTexture (GL_TEXTURE0 + 3 * i);
          gsk_gl_image_bind_texture (GSK_GL_IMAGE (self->images[i]));
          state->current_images[i] = self->images[i];
        }
      if (state->current_samplers[i] != self->samplers[i])
        {
          glBindSampler (3 * i, gsk_gl_device_get_sampler_id (GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                              self->samplers[i]));
          state->current_samplers[i] = self->samplers[i];
        }
    }

  if (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_MERGE))
    max_ops_per_draw = MAX_MERGE_OPS;
  else
    max_ops_per_draw = 1;

  n_ops = self->n_ops;
  for (next = op->next; next; next = next->next)
    {
      GskGpuShaderOp *next_shader = (GskGpuShaderOp *) next;

      if (next->op_class != op->op_class ||
          next_shader->flags != self->flags ||
          next_shader->color_states != self->color_states ||
          next_shader->variation != self->variation ||
          next_shader->vertex_offset != self->vertex_offset + n_ops * shader_op_class->vertex_size ||
          (shader_op_class->n_textures > 0 && (next_shader->images[0] != self->images[0] || next_shader->samplers[0] != self->samplers[0])) ||
          (shader_op_class->n_textures > 1 && (next_shader->images[1] != self->images[1] || next_shader->samplers[1] != self->samplers[1])))
        break;

      n_ops += next_shader->n_ops;
    }

  for (i = 0; i < n_ops; i += max_ops_per_draw)
    {
      if (gdk_gl_context_has_feature (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                                      GDK_GL_FEATURE_BASE_INSTANCE))
        {
          glDrawArraysInstancedBaseInstance (GL_TRIANGLES,
                                             0,
                                             6 * instance_scale,
                                             MIN (max_ops_per_draw, n_ops - i),
                                             self->vertex_offset / shader_op_class->vertex_size + i);
        }
      else
        {
          shader_op_class->setup_vao (self->vertex_offset + i * shader_op_class->vertex_size);

          glDrawArraysInstanced (GL_TRIANGLES,
                                 0,
                                 6 * instance_scale,
                                 MIN (max_ops_per_draw, n_ops - i));
        }
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

void
gsk_gpu_shader_op_alloc (GskGpuFrame               *frame,
                         const GskGpuShaderOpClass *op_class,
                         GskGpuColorStates          color_states,
                         guint32                    variation,
                         GskGpuShaderClip           clip,
                         GskGpuImage              **images,
                         GskGpuSampler             *samplers,
                         gpointer                   out_vertex_data)
{
  GskGpuOp *last;
  GskGpuShaderOp *last_shader;
  gsize i, vertex_offset, vertex_size, texture_vertex_size;
  guchar *vertex_data;
  GskGpuShaderFlags flags;

  flags = gsk_gpu_shader_flags_create (clip,
                                       op_class->n_textures > 0 && (gsk_gpu_image_get_flags (images[0]) & GSK_GPU_IMAGE_EXTERNAL),
                                       op_class->n_textures > 1 && (gsk_gpu_image_get_flags (images[1]) & GSK_GPU_IMAGE_EXTERNAL));
  texture_vertex_size = gsk_gpu_frame_get_texture_vertex_size (frame, op_class->n_textures);
  vertex_size = texture_vertex_size + op_class->vertex_size;
  vertex_offset = gsk_gpu_frame_reserve_vertex_data (frame, vertex_size);
  vertex_data = gsk_gpu_frame_get_vertex_data (frame, vertex_offset);

  gsk_gpu_frame_write_texture_vertex_data (frame,
                                           vertex_data,
                                           images,
                                           samplers,
                                           op_class->n_textures);

  last = gsk_gpu_frame_get_last_op (frame);
  /* careful: We're casting without checking, but the if() does the check */
  last_shader = (GskGpuShaderOp *) last;
  if (last &&
      last->op_class == (const GskGpuOpClass *) op_class &&
      last_shader->color_states == color_states &&
      last_shader->variation == variation &&
      last_shader->flags == flags &&
      last_shader->vertex_offset + last_shader->n_ops * vertex_size == vertex_offset &&
      (op_class->n_textures < 1 || (last_shader->images[0] == images[0] && last_shader->samplers[0] == samplers[0])) &&
      (op_class->n_textures < 2 || (last_shader->images[1] == images[1] && last_shader->samplers[1] == samplers[1])))
    {
      last_shader->n_ops++;
    }
  else
    {
      GskGpuShaderOp *self;
      self = (GskGpuShaderOp *) gsk_gpu_op_alloc (frame, &op_class->parent_class);

      self->flags = flags;
      self->color_states = color_states;
      self->variation = variation;
      self->vertex_offset = vertex_offset;
      self->n_ops = 1;
      for (i = 0; i < op_class->n_textures; i++)
        {
          self->images[i] = g_object_ref (images[i]);
          self->samplers[i] = samplers[i];
        }
    }

  *((gpointer *) out_vertex_data) = vertex_data + texture_vertex_size;
}


#include "config.h"

#include "gskgpushaderopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgldeviceprivate.h"
#include "gskglframeprivate.h"
#include "gskglimageprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkandeviceprivate.h"
#endif

void
gsk_gpu_shader_op_finish (GskGpuOp *op)
{
  GskGpuShaderOp *self = (GskGpuShaderOp *) op;
  gsize i;

  for (i = 0; i < self->n_images; i++)
    g_object_unref (self->images[i].image);
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
  for (next = op->next; next && i < 10 * 1000; next = next->next)
    {
      GskGpuShaderOp *next_shader = (GskGpuShaderOp *) next;
  
      if (next->op_class != op->op_class ||
          next_shader->vertex_offset != self->vertex_offset + i * shader_op_class->vertex_size)
        break;

      i++;
    }

  vkCmdBindPipeline (command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     gsk_vulkan_device_get_vk_pipeline (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
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
  GskGLDevice *device;
  gsize i;

  device = GSK_GL_DEVICE (gsk_gpu_frame_get_device (frame));

  gsk_gl_frame_use_program (GSK_GL_FRAME (frame),
                            shader_op_class,
                            self->clip);

  for (i = 0; i < self->n_images; i++)
    {
      glActiveTexture (GL_TEXTURE0 + self->images[i].descriptor);
      gsk_gl_image_bind_texture (GSK_GL_IMAGE (self->images[i].image));
      glBindSampler (self->images[i].descriptor,
                     gsk_gl_device_get_sampler_id (device, self->images[i].sampler));
    }

  shader_op_class->setup_vao (self->vertex_offset);

  glDrawArraysInstanced (GL_TRIANGLES,
                         0,
                         6 * instance_scale,
                         1);

  return op->next;
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
                         gpointer                   out_vertex_data)
{
  GskGpuShaderOp *self;

  self = (GskGpuShaderOp *) gsk_gpu_op_alloc (frame, &op_class->parent_class);

  self->clip = clip;
  self->vertex_offset = gsk_gpu_frame_reserve_vertex_data (frame, op_class->vertex_size);

  *((gpointer *) out_vertex_data) = gsk_gpu_frame_get_vertex_data (frame, self->vertex_offset);

  return self;
}

guint32
gsk_gpu_shader_op_use_image (GskGpuShaderOp *self,
                             GskGpuFrame    *frame,
                             GskGpuImage    *image,
                             GskGpuSampler   sampler)
{
  gsize id;

  g_assert (self->n_images < G_N_ELEMENTS (self->images));

  id = self->n_images;
  self->images[id].image = g_object_ref (image);
  self->images[id].sampler = sampler;
  self->images[id].descriptor = gsk_gpu_frame_get_image_descriptor (frame, image, sampler);
  self->n_images++;

  return self->images[id].descriptor;
}
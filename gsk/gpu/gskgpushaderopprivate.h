#pragma once

#include "gskgpuopprivate.h"

#include "gskgputypesprivate.h"
#include "gskgpudescriptorsprivate.h"
#include "gskgpuframeprivate.h"

G_BEGIN_DECLS

struct _GskGpuShaderOp
{
  GskGpuOp parent_op;

  GskGpuDescriptors *desc;
  guint32 variation;
  GskGpuShaderClip clip;
  gsize vertex_offset;
};

struct _GskGpuShaderOpClass
{
  GskGpuOpClass         parent_class;

  const char *          shader_name;
  gsize                 vertex_size;
#ifdef GDK_RENDERING_VULKAN
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state;
#endif
  void                  (* setup_attrib_locations)                      (GLuint                  program);
  void                  (* setup_vao)                                   (gsize                   offset);
};

void                    gsk_gpu_shader_op_finish                        (GskGpuOp               *op);

#ifdef GDK_RENDERING_VULKAN
GskGpuOp *              gsk_gpu_shader_op_vk_command_n                  (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskVulkanCommandState  *state,
                                                                         gsize                   instance_scale);
GskGpuOp *              gsk_gpu_shader_op_vk_command                    (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskVulkanCommandState  *state);
#endif
GskGpuOp *              gsk_gpu_shader_op_gl_command_n                  (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskGLCommandState      *state,
                                                                         gsize                   instance_scale);
GskGpuOp *              gsk_gpu_shader_op_gl_command                    (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskGLCommandState      *state);

static inline void
gsk_gpu_rgba_to_float (const GdkRGBA *rgba,
                       float          values[4])
{
  values[0] = rgba->red;
  values[1] = rgba->green;
  values[2] = rgba->blue;
  values[3] = rgba->alpha;
}

#include <graphene.h>

static inline void
gsk_gpu_point_to_float (const graphene_point_t *point,
                        const graphene_point_t *offset,
                        float                   values[2])
{
  values[0] = point->x + offset->x;
  values[1] = point->y + offset->y;
}

static inline GskGpuShaderOp *
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
    self->desc = gsk_gpu_descriptors_ref (desc);
  else
    self->desc = NULL;
  self->vertex_offset = gsk_gpu_frame_reserve_vertex_data (frame, op_class->vertex_size);

  *((gpointer *) out_vertex_data) = gsk_gpu_frame_get_vertex_data (frame, self->vertex_offset);

  return self;
}

G_END_DECLS


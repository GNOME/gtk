#pragma once

#include "gskgpuopprivate.h"

#include "gskgputypesprivate.h"
#include "gskgpucolorstatesprivate.h"

G_BEGIN_DECLS

struct _GskGpuShaderOp
{
  GskGpuOp parent_op;

  GskGpuDescriptors *desc;
  GskGpuColorStates color_states;
  guint32 variation;
  GskGpuShaderClip clip;
  gsize vertex_offset;
  gsize n_ops;
};

struct _GskGpuShaderOpClass
{
  GskGpuOpClass         parent_class;

  const char *          shader_name;
  gsize                 vertex_size;
#ifdef GDK_RENDERING_VULKAN
  const VkPipelineVertexInputStateCreateInfo *vertex_input_state;
#endif
  void                  (* print_instance)                              (GskGpuShaderOp         *shader,
                                                                         gpointer                instance,
                                                                         GString                *string);
  void                  (* setup_attrib_locations)                      (GLuint                  program);
  void                  (* setup_vao)                                   (gsize                   offset);
};

void                    gsk_gpu_shader_op_alloc                         (GskGpuFrame            *frame,
                                                                         const GskGpuShaderOpClass *op_class,
                                                                         GskGpuColorStates       color_states,
                                                                         guint32                 variation,
                                                                         GskGpuShaderClip        clip,
                                                                         GskGpuDescriptors      *desc,
                                                                         gpointer                out_vertex_data);

void                    gsk_gpu_shader_op_finish                        (GskGpuOp               *op);

void                    gsk_gpu_shader_op_print                         (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GString                *string,
                                                                         guint                   indent);
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

#define GSK_RGBA_TO_VEC4(_color) (float[4]) { (_color)->red, (_color)->green, (_color)->blue, (_color)->alpha }
#define GSK_RGBA_TO_VEC4_ALPHA(_color, _alpha) (float[4]) { (_color)->red, (_color)->green, (_color)->blue, (_color)->alpha * (_alpha) }

static inline void
gsk_gpu_color_to_float (const float color[4],
                        float       values[4])
{
  values[0] = color[0];
  values[1] = color[1];
  values[2] = color[2];
  values[3] = color[3];
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

G_END_DECLS


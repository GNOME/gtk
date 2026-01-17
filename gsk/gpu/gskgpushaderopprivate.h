#pragma once

#include "gskgpuopprivate.h"

#include "gskgputypesprivate.h"
#include "gskgpucolorstatesprivate.h"
#include "gdkcolorprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

struct _GskGpuShaderOp
{
  GskGpuOp parent_op;

  GskGpuImage *images[2];
  GskGpuSampler samplers[2];
  GskGpuShaderFlags flags;
  GskGpuColorStates color_states;
  guint32 variation;
  gsize vertex_offset;
  gsize n_ops;
};

struct _GskGpuShaderOpClass
{
  GskGpuOpClass         parent_class;

  const char *          shader_name;
  gsize                 n_textures;
  gsize                 n_instances;
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
                                                                         GskGpuImage           **images,
                                                                         GskGpuSampler          *samplers,
                                                                         gpointer                out_vertex_data);

void                    gsk_gpu_shader_op_finish                        (GskGpuOp               *op);

void                    gsk_gpu_shader_op_print                         (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GString                *string,
                                                                         guint                   indent);
#ifdef GDK_RENDERING_VULKAN
GskGpuOp *              gsk_gpu_shader_op_vk_command                    (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskVulkanCommandState  *state);
#endif
GskGpuOp *              gsk_gpu_shader_op_gl_command                    (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskGLCommandState      *state);

#define GSK_RGBA_TO_VEC4(_color) (float[4]) { (_color)->red, (_color)->green, (_color)->blue, (_color)->alpha }
#define GSK_RGBA_TO_VEC4_ALPHA(_color, _alpha) (float[4]) { (_color)->red, (_color)->green, (_color)->blue, (_color)->alpha * (_alpha) }
#define GSK_VEC4_TRANSPARENT (float[4]) { 0.0f, 0.0f, 0.0f, 0.0f }

static inline void
gsk_gpu_vec4_to_float (const float color[4],
                       float       values[4])
{
  values[0] = color[0];
  values[1] = color[1];
  values[2] = color[2];
  values[3] = color[3];
}

G_END_DECLS

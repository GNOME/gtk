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
#ifdef GDK_WINDOWING_WIN32
  const D3D12_INPUT_LAYOUT_DESC              *d3d12_input_layout;
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
#ifdef GDK_WINDOWING_WIN32
GskGpuOp *              gsk_gpu_shader_op_d3d12_command                 (GskGpuOp               *op,
                                                                         GskGpuFrame            *frame,
                                                                         GskD3d12CommandState   *state);
#endif

#define GSK_RGBA_TO_VEC4(_color) (float[4]) { (_color)->red, (_color)->green, (_color)->blue, (_color)->alpha }
#define GSK_RGBA_TO_VEC4_ALPHA(_color, _alpha) (float[4]) { (_color)->red, (_color)->green, (_color)->blue, (_color)->alpha * (_alpha) }
#define GSK_VEC4_TRANSPARENT (float[4]) { 0.0f, 0.0f, 0.0f, 0.0f }

G_END_DECLS

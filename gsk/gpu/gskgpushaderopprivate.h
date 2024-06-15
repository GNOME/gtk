#pragma once

#include "gskgpuopprivate.h"

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

struct _GskGpuShaderOp
{
  GskGpuOp parent_op;

  GskGpuDescriptors *desc;
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
gsk_gpu_color_to_float (const GdkColor *color,
                        float           values[4])
{
  values[0] = color->values[0];
  values[1] = color->values[1];
  values[2] = color->values[2];
  values[3] = color->values[3];
}

static inline void
gsk_gpu_convert_color_to_float (const GdkColor *color,
                                GdkColorState  *in,
                                float           values[4])
{
  GdkColor c;

  gdk_color_convert (&c, in, color);
  gsk_gpu_color_to_float (&c, values);
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


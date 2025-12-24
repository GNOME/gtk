#include "config.h"

#include "gskgpudisplacementopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpudisplacementinstance.h"

typedef struct _GskGpuDisplacementOp GskGpuDisplacementOp;

struct _GskGpuDisplacementOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_displacement_op_print_instance (GskGpuShaderOp *shader,
                                        gpointer        instance_,
                                        GString        *string)
{
  GskGpuDisplacementInstance *instance = (GskGpuDisplacementInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  gsk_gpu_print_image (string, shader->images[1]);
  g_string_append_printf (string, "%c%c ", "RGBA"[instance->channels[0]], "RGBA"[instance->channels[1]]);
}

static const GskGpuShaderOpClass GSK_GPU_DISPLACEMENT_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuDisplacementOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command,
#ifdef GDK_WINDOWING_WIN32
    gsk_gpu_shader_op_d3d12_command
#endif
  },
  "gskgpudisplacement",
  gsk_gpu_displacement_n_textures,
  sizeof (GskGpuDisplacementInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_displacement_info,
#endif
#ifdef GDK_WINDOWING_WIN32
  &gsk_gpu_displacement_input_layout,
#endif
  gsk_gpu_displacement_op_print_instance,
  gsk_gpu_displacement_setup_attrib_locations,
  gsk_gpu_displacement_setup_vao
};

void
gsk_gpu_displacement_op (GskGpuFrame             *frame,
                         GskGpuShaderClip         clip,
                         const graphene_point_t  *offset,
                         float                    opacity,
                         const graphene_rect_t   *bounds,
                         const GskGpuShaderImage *child,
                         const GskGpuShaderImage *displacement,
                         const guint              channels[2],
                         const graphene_size_t   *max,
                         const graphene_size_t   *scale,
                         const graphene_point_t  *offset2)
{
  GskGpuDisplacementInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_DISPLACEMENT_OP_CLASS,
                           gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           0,
                           clip,
                           (GskGpuImage *[2]) { displacement->image, child->image },
                           (GskGpuSampler[2]) { displacement->sampler, child->sampler },
                           &instance);

  gsk_gpu_rect_to_float (bounds, offset, instance->rect);
  gsk_gpu_rect_to_float (displacement->bounds, offset, instance->displacement_rect);
  gsk_gpu_rect_to_float (child->bounds, offset, instance->child_rect);
  instance->channels[0] = channels[0];
  instance->channels[1] = channels[1];
  instance->max[0] = max->width;
  instance->max[1] = max->height;
  instance->scale[0] = scale->width;
  instance->scale[1] = scale->height;
  instance->offset[0] = offset2->x;
  instance->offset[1] = offset2->y;
  instance->opacity = opacity;
}

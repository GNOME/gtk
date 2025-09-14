#include "config.h"

#include "gskgpucomponenttransferopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"
#include "gskcomponenttransferprivate.h"

#include "gpu/shaders/gskgpucomponenttransferinstance.h"

typedef struct _GskGpuComponentTransferOp GskGpuComponentTransferOp;

struct _GskGpuComponentTransferOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_component_transfer_op_print_instance (GskGpuShaderOp *shader,
                                              gpointer        instance_,
                                              GString        *string)
{
  GskGpuComponenttransferInstance *instance = (GskGpuComponenttransferInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
}

static const GskGpuShaderOpClass GSK_GPU_COMPONENT_TRANSFER_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuComponentTransferOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucomponenttransfer",
  gsk_gpu_componenttransfer_n_textures,
  sizeof (GskGpuComponenttransferInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_componenttransfer_info,
#endif
  gsk_gpu_component_transfer_op_print_instance,
  gsk_gpu_componenttransfer_setup_attrib_locations,
  gsk_gpu_componenttransfer_setup_vao
};

static void
set_param (GskGpuComponenttransferInstance *instance,
           guint                            coord,
           guint                            idx,
           float                            value)
{
  switch (coord)
    {
    case 0:
      instance->params_r[idx] = value;
      break;
    case 1:
      instance->params_g[idx] = value;
      break;
    case 2:
      instance->params_b[idx] = value;
      break;
    case 3:
      instance->params_a[idx] = value;
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
set_table_value (GskGpuComponenttransferInstance *instance,
                 guint                            idx,
                 float                            value)
{
  if (idx < 4)
    instance->table0[idx] = value;
  else if (idx < 8)
    instance->table1[idx - 4] = value;
  else if (idx < 12)
    instance->table2[idx - 8] = value;
  else if (idx < 16)
    instance->table3[idx - 12] = value;
  else if (idx < 20)
    instance->table4[idx - 16] = value;
  else if (idx < 24)
    instance->table5[idx - 20] = value;
  else if (idx < 28)
    instance->table6[idx - 24] = value;
  else if (idx < 32)
    instance->table7[idx - 28] = value;
}

static void
copy_component_transfer (const GskComponentTransfer      *transfer,
                         guint                            coord,
                         GskGpuComponenttransferInstance *instance,
                         guint                           *n)
{
  set_param (instance, coord, 0, transfer->kind);
  switch (transfer->kind)
    {
    case GSK_COMPONENT_TRANSFER_IDENTITY:
      break;
    case GSK_COMPONENT_TRANSFER_LEVELS:
      set_param (instance, coord, 1, transfer->levels.n);
      break;
    case GSK_COMPONENT_TRANSFER_LINEAR:
      set_param (instance, coord, 1, transfer->linear.m);
      set_param (instance, coord, 2, transfer->linear.b);
      break;
    case GSK_COMPONENT_TRANSFER_GAMMA:
      set_param (instance, coord, 1, transfer->gamma.amp);
      set_param (instance, coord, 2, transfer->gamma.exp);
      set_param (instance, coord, 3, transfer->gamma.ofs);
      break;
    case GSK_COMPONENT_TRANSFER_DISCRETE:
    case GSK_COMPONENT_TRANSFER_TABLE:
      if (*n + transfer->table.n >= 32)
        g_warning ("tables too big in component transfer");
      set_param (instance, coord, 1, transfer->table.n);
      set_param (instance, coord, 2, *n);
      for (guint i = 0; i < transfer->table.n; i++)
        set_table_value (instance, *n + i, transfer->table.values[i]);
      *n += transfer->table.n;
      break;
    default:
      g_assert_not_reached ();
    }
}

void
gsk_gpu_component_transfer_op (GskGpuFrame                *frame,
                               GskGpuShaderClip            clip,
                               GskGpuColorStates           color_states,
                               const graphene_point_t     *offset,
                               float                       opacity,
                               const GskGpuShaderImage    *image,
                               const GskComponentTransfer *red,
                               const GskComponentTransfer *green,
                               const GskComponentTransfer *blue,
                               const GskComponentTransfer *alpha)
{
  GskGpuComponenttransferInstance *instance;
  guint n;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_COMPONENT_TRANSFER_OP_CLASS,
                           color_states,
                           0,
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);

  n = 0;
  copy_component_transfer (red, 0, instance, &n);
  copy_component_transfer (green, 1, instance, &n);
  copy_component_transfer (blue, 2, instance, &n);
  copy_component_transfer (alpha, 3, instance, &n);

  instance->opacity = opacity;
}


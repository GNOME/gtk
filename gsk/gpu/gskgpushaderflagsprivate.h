#pragma once

#include "gskgputypesprivate.h"

static inline GskGpuShaderFlags
gsk_gpu_shader_flags_create (GskGpuShaderClip clip,
                             GdkShaderOp      texture0_shader_op,
                             gboolean         texture0_is_external,
                             GdkShaderOp      texture1_shader_op,
                             gboolean         texture1_is_external)
{
  return (clip & GSK_GPU_SHADER_CLIP_MASK) |
         (texture0_is_external ? 1 << GSK_GPU_SHADER_CLIP_SHIFT : 0) | 
         (texture1_is_external ? 1 << (GSK_GPU_SHADER_CLIP_SHIFT + 1) : 0) |
         (texture0_shader_op << (GSK_GPU_SHADER_CLIP_SHIFT + 2)) |
         (texture1_shader_op << (GSK_GPU_SHADER_CLIP_SHIFT + 2 + GSK_GPU_SHADER_OP_SHIFT));
}

static inline gboolean
gsk_gpu_shader_flags_has_external_textures (GskGpuShaderFlags flags)
{
  return flags & (3 << GSK_GPU_SHADER_CLIP_SHIFT) ? TRUE : FALSE;
}

static inline gboolean
gsk_gpu_shader_flags_has_external_texture0 (GskGpuShaderFlags flags)
{
  return flags & (1 << GSK_GPU_SHADER_CLIP_SHIFT) ? TRUE : FALSE;
}

static inline gboolean
gsk_gpu_shader_flags_has_external_texture1 (GskGpuShaderFlags flags)
{
  return flags & (2 << GSK_GPU_SHADER_CLIP_SHIFT) ? TRUE : FALSE;
}

static inline GskGpuShaderClip
gsk_gpu_shader_flags_get_clip (GskGpuShaderFlags flags)
{
  return flags & GSK_GPU_SHADER_CLIP_MASK;
}

static inline GdkShaderOp
gsk_gpu_shader_flags_get_texture0_op (GskGpuShaderFlags flags)
{
  return flags >> (GSK_GPU_SHADER_CLIP_SHIFT + 2) & GSK_GPU_SHADER_OP_MASK;
}

static inline GdkShaderOp
gsk_gpu_shader_flags_get_texture1_op (GskGpuShaderFlags flags)
{
  return flags >> (GSK_GPU_SHADER_CLIP_SHIFT + 2 + GSK_GPU_SHADER_OP_SHIFT) & GSK_GPU_SHADER_OP_MASK;
}

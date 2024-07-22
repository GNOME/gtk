#pragma once

#include "gskgputypesprivate.h"

static inline GskGpuShaderFlags
gsk_gpu_shader_flags_create (GskGpuShaderClip clip,
                             gboolean         texture0_is_external,
                             gboolean         texture1_is_external)
{
  return (clip & GSK_GPU_SHADER_CLIP_MASK) |
         (texture0_is_external ? 1 << GSK_GPU_SHADER_CLIP_SHIFT : 0) | 
         (texture1_is_external ? 1 << (GSK_GPU_SHADER_CLIP_SHIFT + 1) : 0);
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

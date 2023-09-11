#pragma once

#include <gdk/gdk.h>

#include "gdk/gdkmemoryformatprivate.h"

typedef struct _GskGpuBuffer            GskGpuBuffer;
typedef struct _GskGpuDevice            GskGpuDevice;
typedef struct _GskGpuFrame             GskGpuFrame;
typedef struct _GskGpuImage             GskGpuImage;
typedef struct _GskGpuOp                GskGpuOp;
typedef struct _GskGpuOpClass           GskGpuOpClass;
typedef struct _GskGpuShaderOp          GskGpuShaderOp;
typedef struct _GskGpuShaderOpClass     GskGpuShaderOpClass;

typedef enum {
  GSK_GPU_SAMPLER_DEFAULT,
  GSK_GPU_SAMPLER_REPEAT,
  GSK_GPU_SAMPLER_NEAREST,
  /* add more */
  GSK_GPU_SAMPLER_N_SAMPLERS
} GskGpuSampler;

typedef enum {
  GSK_GPU_SHADER_CLIP_NONE,
  GSK_GPU_SHADER_CLIP_RECT,
  GSK_GPU_SHADER_CLIP_ROUNDED
} GskGpuShaderClip;

typedef enum {
  GSK_GPU_PATTERN_DONE,
  GSK_GPU_PATTERN_COLOR,
  GSK_GPU_PATTERN_OPACITY,
  GSK_GPU_PATTERN_TEXTURE,
  GSK_GPU_PATTERN_COLOR_MATRIX,
  GSK_GPU_PATTERN_GLYPHS,
  GSK_GPU_PATTERN_LINEAR_GRADIENT,
  GSK_GPU_PATTERN_REPEATING_LINEAR_GRADIENT,
  GSK_GPU_PATTERN_RADIAL_GRADIENT,
  GSK_GPU_PATTERN_REPEATING_RADIAL_GRADIENT,
  GSK_GPU_PATTERN_CONIC_GRADIENT,
  GSK_GPU_PATTERN_CLIP,
  GSK_GPU_PATTERN_ROUNDED_CLIP,
} GskGpuPatternType;


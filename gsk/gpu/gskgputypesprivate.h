#pragma once

#include <gdk/gdk.h>

#include "gdk/gdkmemoryformatprivate.h"

#define GSK_GPU_PATTERN_STACK_SIZE 16

typedef struct _GskGpuBuffer            GskGpuBuffer;
typedef struct _GskGpuDescriptors       GskGpuDescriptors;
typedef struct _GskGpuDevice            GskGpuDevice;
typedef struct _GskGpuFrame             GskGpuFrame;
typedef struct _GskGpuImage             GskGpuImage;
typedef struct _GskGpuOp                GskGpuOp;
typedef struct _GskGpuOpClass           GskGpuOpClass;
typedef struct _GskGpuShaderOp          GskGpuShaderOp;
typedef struct _GskGpuShaderOpClass     GskGpuShaderOpClass;

typedef enum {
  GSK_GPU_IMAGE_EXTERNAL       = (1 << 0),
  GSK_GPU_IMAGE_TOGGLE_REF     = (1 << 1),
} GskGpuImageFlags;

typedef enum {
  GSK_GPU_SAMPLER_DEFAULT,
  GSK_GPU_SAMPLER_TRANSPARENT,
  GSK_GPU_SAMPLER_REPEAT,
  GSK_GPU_SAMPLER_NEAREST,
  GSK_GPU_SAMPLER_MIPMAP_DEFAULT,
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
  GSK_GPU_PATTERN_REPEAT_PUSH,
  GSK_GPU_PATTERN_POSITION_POP,
  GSK_GPU_PATTERN_PUSH_COLOR,
  GSK_GPU_PATTERN_POP_CROSS_FADE,
  GSK_GPU_PATTERN_POP_MASK_ALPHA,
  GSK_GPU_PATTERN_POP_MASK_INVERTED_ALPHA,
  GSK_GPU_PATTERN_POP_MASK_LUMINANCE,
  GSK_GPU_PATTERN_POP_MASK_INVERTED_LUMINANCE,
} GskGpuPatternType;

typedef enum {
  GSK_GPU_OPTIMIZE_UBER                 = 1 <<  0,
  GSK_GPU_OPTIMIZE_CLEAR                = 1 <<  1,
  /* These require hardware support */
  GSK_GPU_OPTIMIZE_GL_BASE_INSTANCE     = 1 <<  2,
} GskGpuOptimizations;


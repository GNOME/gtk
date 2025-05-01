#pragma once

#include "gskgputypesprivate.h"

GskGpuConversion        gsk_gpu_color_state_get_conversion              (GdkColorState                  *color_state);
GdkColorState *         gsk_gpu_color_state_apply_conversion            (GdkColorState                  *color_state,
                                                                         GskGpuConversion                conversion);

static inline guint
gsk_gpu_mipmap_levels (gsize width,
                       gsize height)
{
  return g_bit_nth_msf (MAX (width, height), -1) + 1;
}
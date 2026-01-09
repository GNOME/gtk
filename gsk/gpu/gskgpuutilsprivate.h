#pragma once

#include "gskgputypesprivate.h"
#include "gskrendernodeprivate.h"

GskGpuConversion        gsk_gpu_color_state_get_conversion              (GdkColorState                  *color_state);
GdkColorState *         gsk_gpu_color_state_apply_conversion            (GdkColorState                  *color_state,
                                                                         GskGpuConversion                conversion);

void                    gsk_gpu_color_stops_to_shader                   (const GskGradientStop          *stops,
                                                                         gsize                           n_stops,
                                                                         GdkColorState                  *color_state,
                                                                         GskHueInterpolation             interp,
                                                                         GdkColor                        colors[7],
                                                                         graphene_vec4_t                 offsets[2],
                                                                         graphene_vec4_t                 hints[2]);

static inline guint
gsk_gpu_mipmap_levels (gsize width,
                       gsize height)
{
  return g_bit_nth_msf (MAX (width, height), -1) + 1;
}

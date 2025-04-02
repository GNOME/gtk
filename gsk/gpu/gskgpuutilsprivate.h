#pragma once

#include "gskgputypesprivate.h"

GskGpuConversion        gsk_gpu_color_state_get_conversion              (GdkColorState                  *color_state);
GdkColorState *         gsk_gpu_color_state_apply_conversion            (GdkColorState                  *color_state,
                                                                         GskGpuConversion                conversion);

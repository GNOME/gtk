#pragma once

#include "gskgputypesprivate.h"

#include "gdk/gdkcolorstateprivate.h"

#define COLOR_SPACE_OUTPUT_PREMULTIPLIED (1u << 2)
#define COLOR_SPACE_ALT_PREMULTIPLIED    (1u << 3)
#define COLOR_SPACE_OUTPUT_SHIFT 8u
#define COLOR_SPACE_ALT_SHIFT 16u
#define COLOR_SPACE_COLOR_STATE_MASK 0xFFu

static inline GskGpuColorStates
gsk_gpu_color_states_create (GdkColorState *output_color_state,
                             gboolean       output_is_premultiplied,
                             GdkColorState *alt_color_state,
                             gboolean       alt_is_premultiplied)
{
  g_assert (GDK_IS_DEFAULT_COLOR_STATE (output_color_state));
  g_assert (GDK_IS_DEFAULT_COLOR_STATE (alt_color_state));

  return (GDK_DEFAULT_COLOR_STATE_ID (output_color_state) << COLOR_SPACE_OUTPUT_SHIFT) |
         (output_is_premultiplied ? COLOR_SPACE_OUTPUT_PREMULTIPLIED : 0) |
         (GDK_DEFAULT_COLOR_STATE_ID (alt_color_state) << COLOR_SPACE_ALT_SHIFT) |
         (alt_is_premultiplied ? COLOR_SPACE_ALT_PREMULTIPLIED : 0);
}

static inline GdkColorState *
gsk_gpu_color_states_get_output (GskGpuColorStates self)
{
  return ((GdkColorState *) &gdk_default_color_states[(self >> COLOR_SPACE_OUTPUT_SHIFT) & COLOR_SPACE_COLOR_STATE_MASK]);
}

static inline gboolean
gsk_gpu_color_states_is_output_premultiplied (GskGpuColorStates self)
{
  return !!(self & COLOR_SPACE_OUTPUT_PREMULTIPLIED);
}

static inline GdkColorState *
gsk_gpu_color_states_get_alt (GskGpuColorStates self)
{
  return ((GdkColorState *) &gdk_default_color_states[(self >> COLOR_SPACE_ALT_SHIFT) & COLOR_SPACE_COLOR_STATE_MASK]);
}

static inline gboolean
gsk_gpu_color_states_is_alt_premultiplied (GskGpuColorStates self)
{
  return !!(self & COLOR_SPACE_ALT_PREMULTIPLIED);
}

#define DEFAULT_COLOR_STATES (gsk_gpu_color_states_create (GDK_COLOR_STATE_SRGB, TRUE, GDK_COLOR_STATE_SRGB, TRUE))

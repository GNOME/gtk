#pragma once

#include "gskgputypesprivate.h"

#include "gdk/gdkcolorprivate.h"
#include "gdk/gdkcolorstateprivate.h"

#define COLOR_SPACE_OUTPUT_PREMULTIPLIED (1u << 2)
#define COLOR_SPACE_ALT_PREMULTIPLIED    (1u << 3)
#define COLOR_SPACE_OUTPUT_SHIFT 8u
#define COLOR_SPACE_ALT_SHIFT 16u
#define COLOR_SPACE_COLOR_STATE_MASK 0xFFu

static inline GskGpuColorStates
gsk_gpu_color_states_create_equal (gboolean output_is_premultiplied,
                                   gboolean alt_is_premultiplied)
{
  /* We use ID 0 here for the colorspaces - if that ever becomes an issue
   * that it maps to SRGB, we need to invent something */
  return (output_is_premultiplied ? COLOR_SPACE_OUTPUT_PREMULTIPLIED : 0) |
         (alt_is_premultiplied ? COLOR_SPACE_ALT_PREMULTIPLIED : 0);
}

static inline GskGpuColorStates
gsk_gpu_color_states_create (GdkColorState *output_color_state,
                             gboolean       output_is_premultiplied,
                             GdkColorState *alt_color_state,
                             gboolean       alt_is_premultiplied)
{
  g_assert (GDK_IS_DEFAULT_COLOR_STATE (output_color_state));
  g_assert (GDK_IS_DEFAULT_COLOR_STATE (alt_color_state));

  if (gdk_color_state_equal (output_color_state, alt_color_state))
    return gsk_gpu_color_states_create_equal (output_is_premultiplied, alt_is_premultiplied);

  return (GDK_DEFAULT_COLOR_STATE_ID (output_color_state) << COLOR_SPACE_OUTPUT_SHIFT) |
         (output_is_premultiplied ? COLOR_SPACE_OUTPUT_PREMULTIPLIED : 0) |
         (GDK_DEFAULT_COLOR_STATE_ID (alt_color_state) << COLOR_SPACE_ALT_SHIFT) |
         (alt_is_premultiplied ? COLOR_SPACE_ALT_PREMULTIPLIED : 0);
}

static inline GskGpuColorStates
gsk_gpu_color_states_create_cicp (GdkColorState *output_color_state,
                                  gboolean       output_is_premultiplied,
                                  gboolean       cicp_is_premultiplied)
{
  g_assert (GDK_IS_DEFAULT_COLOR_STATE (output_color_state));

  return (GDK_DEFAULT_COLOR_STATE_ID (output_color_state) << COLOR_SPACE_OUTPUT_SHIFT) |
         (output_is_premultiplied ? COLOR_SPACE_OUTPUT_PREMULTIPLIED : 0) |
         (GDK_DEFAULT_COLOR_STATE_ID (output_color_state) << COLOR_SPACE_ALT_SHIFT) |
         (cicp_is_premultiplied ? COLOR_SPACE_ALT_PREMULTIPLIED : 0);
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

/* Note: this function should only return a colorstate other than
 * color->color_state *only* if the shaders can't handle the conversion
 * from color->color_state to ccs.
 */
static inline GdkColorState *
gsk_gpu_color_states_find (GdkColorState  *ccs,
                           const GdkColor *color)
{
  if (GDK_IS_DEFAULT_COLOR_STATE (color->color_state))
    return color->color_state;
  else
    return ccs;
}

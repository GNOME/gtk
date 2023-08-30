#ifndef _PATTERN_
#define _PATTERN_

#include "common.glsl"

#ifdef GSK_FRAGMENT_SHADER

vec4
gsk_get_vec4 (uint id)
{
  return vec4 (gsk_get_float (id),
               gsk_get_float (id + 1),
               gsk_get_float (id + 2),
               gsk_get_float (id + 3));
}

vec4
color_pattern (uint pattern)
{
  vec4 color = gsk_get_vec4 (pattern);

  return color_premultiply (color);
}

vec4
pattern (uint pattern,
         vec2 pos)
{
  uint type = gsk_get_uint (pattern);

  switch (type)
  {
    case GSK_GPU_PATTERN_COLOR:
      return color_pattern (pattern + 1);
    default:
      return vec4 (1.0, 0.0, 0.8, 1.0); /* pink */
  }
}

#endif

#endif

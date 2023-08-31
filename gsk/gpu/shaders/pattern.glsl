#ifndef _PATTERN_
#define _PATTERN_

#include "common.glsl"

#ifdef GSK_FRAGMENT_SHADER

uint
read_uint (inout uint reader)
{
  uint result = gsk_get_uint (reader);
  reader++;
  return result;
}

float
read_float (inout uint reader)
{
  float result = gsk_get_float (reader);
  reader++;
  return result;
}

vec4
read_vec4 (inout uint reader)
{
  return vec4 (read_float (reader), read_float (reader), read_float (reader), read_float (reader));
}

void
opacity_pattern (inout uint reader,
                 inout vec4 color,
                 vec2       pos)
{
  float opacity = read_float (reader);

  color *= opacity;
}

vec4
color_pattern (inout uint reader)
{
  vec4 color = read_vec4 (reader);

  return color_premultiply (color);
}

vec4
pattern (uint reader,
         vec2 pos)
{
  vec4 color = vec4 (1.0, 0.0, 0.8, 1.0); /* pink */

  for(;;)
    {
      uint type = read_uint (reader);
      switch (type)
      {
        default:
        case GSK_GPU_PATTERN_DONE:
          return color;
        case GSK_GPU_PATTERN_COLOR:
          color = color_pattern (reader);
          break;
        case GSK_GPU_PATTERN_OPACITY:
          opacity_pattern (reader, color, pos);
          break;
      }
    }
}

#endif

#endif

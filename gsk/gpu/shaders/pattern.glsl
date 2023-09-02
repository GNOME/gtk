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

mat4
read_mat4 (inout uint reader)
{
  mat4 result = mat4 (gsk_get_float (reader + 0), gsk_get_float (reader + 1),
                      gsk_get_float (reader + 2), gsk_get_float (reader + 3),
                      gsk_get_float (reader + 4), gsk_get_float (reader + 5),
                      gsk_get_float (reader + 6), gsk_get_float (reader + 7),
                      gsk_get_float (reader + 8), gsk_get_float (reader + 9),
                      gsk_get_float (reader + 10), gsk_get_float (reader + 11),
                      gsk_get_float (reader + 12), gsk_get_float (reader + 13),
                      gsk_get_float (reader + 14), gsk_get_float (reader + 15));
  reader += 16;
  return result;
}

void
opacity_pattern (inout uint reader,
                 inout vec4 color,
                 vec2       pos)
{
  float opacity = read_float (reader);

  color *= opacity;
}

void
color_matrix_pattern (inout uint reader,
                      inout vec4 color,
                      vec2       pos)
{
  mat4 matrix = read_mat4 (reader);
  vec4 offset = read_vec4 (reader);

  color = color_unpremultiply (color);

  color = matrix * color + offset;
  color = clamp(color, 0.0, 1.0);

  color = color_premultiply (color);
}

vec4
texture_pattern (inout uint reader,
                 vec2       pos)
{
  uint tex_id = read_uint (reader);
  vec4 tex_rect = read_vec4 (reader);

  return texture (gsk_get_texture (tex_id), (pos - push.scale * tex_rect.xy) / (push.scale * tex_rect.zw));
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
        case GSK_GPU_PATTERN_TEXTURE:
          color = texture_pattern (reader, pos);
          break;
        case GSK_GPU_PATTERN_COLOR_MATRIX:
          color_matrix_pattern (reader, color, pos);
          break;
        case GSK_GPU_PATTERN_OPACITY:
          opacity_pattern (reader, color, pos);
          break;
      }
    }
}

#endif

#endif

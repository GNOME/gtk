#version 420 core

#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in flat mat4 inColorMatrix;
layout(location = 6) in flat vec4 inColorOffset;

layout(set = 0, binding = 0) uniform sampler2D inTexture;

layout(location = 0) out vec4 color;

vec4
color_matrix (vec4 color, mat4 color_matrix, vec4 color_offset)
{
  /* unpremultiply */
  if (color.a != 0.0)
    color.rgb /= color.a;

  color = color_matrix * color + color_offset;
  color = clamp(color, 0.0, 1.0);

  /* premultiply */
  if (color.a != 0.0)
    color.rgb *= color.a;

  return color;
}

void main()
{
  color = clip (inPos, color_matrix (texture (inTexture, inTexCoord), inColorMatrix, inColorOffset));
}

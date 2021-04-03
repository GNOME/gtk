#version 420 core

#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in flat vec2 inSize;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in float inRadius;

layout(set = 0, binding = 0) uniform sampler2D inTexture;

layout(location = 0) out vec4 color;

const int   samples_x    = 15;  // must be odd
const int   samples_y    = 15;  // must be odd

const int   half_samples_x = samples_x / 2;
const int   half_samples_y = samples_y / 2;

float Gaussian (float sigma, float x)
{
  return exp ( - (x * x) / (2.0 * sigma * sigma));
}

vec4 blur_pixel (in vec2 uv)
{
  float total = 0.0;
  vec4 ret = vec4 (0);
  float pixel_size_x = (1.0 / inSize.x);
  float pixel_size_y = (1.0 / inSize.y);

  for (int y = 0; y < samples_y; ++y)
    {
      float fy = Gaussian (inRadius, float(y) - float(half_samples_x));
      float offset_y = float(y - half_samples_y) * pixel_size_y;
      for (int x = 0; x < samples_x; ++x)
        {
          float fx = Gaussian (inRadius, float(x) - float(half_samples_x));
          float offset_x = float(x - half_samples_x) * pixel_size_x;
          total += fx * fy;
          ret += texture(inTexture, uv + vec2(offset_x, offset_y)) * fx * fy;
        }
    }
  return ret / total;
}

void main()
{
  color = clip (inPos, blur_pixel (inTexCoord));
}

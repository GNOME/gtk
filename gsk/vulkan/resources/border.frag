#version 450

#include "clip.frag.glsl"
#include "rounded-rect.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in RoundedRect inRect;
layout(location = 5) in vec4 inBorderWidths;

layout(location = 0) out vec4 color;

void main()
{
  RoundedRect routside = inRect;
  RoundedRect rinside = rounded_rect_shrink (routside, inBorderWidths);
  
  float alpha = clamp (rounded_rect_coverage (routside, inPos) -
                       rounded_rect_coverage (rinside, inPos),
                       0.0, 1.0);
  color = clip_scaled (inPos, vec4(inColor.rgb * inColor.a, inColor.a) * alpha);
}

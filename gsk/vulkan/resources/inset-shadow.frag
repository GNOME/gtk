#version 420 core

#include "clip.frag.glsl"
#include "rounded-rect.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in flat vec4 inOutline;
layout(location = 2) in flat vec4 inOutlineCornerWidths;
layout(location = 3) in flat vec4 inOutlineCornerHeights;
layout(location = 4) in flat vec4 inColor;
layout(location = 5) in flat vec2 inOffset;
layout(location = 6) in flat float inSpread;

layout(location = 0) out vec4 color;

void main()
{
  RoundedRect outline = RoundedRect (vec4(inOutline.xy, inOutline.xy + inOutline.zw), inOutlineCornerWidths, inOutlineCornerHeights);
  RoundedRect inside = rounded_rect_shrink (outline, vec4(inSpread));

  color = vec4(inColor.rgb * inColor.a, inColor.a);
  color = color * clamp (rounded_rect_coverage (outline, inPos) -
                         rounded_rect_coverage (inside, inPos - inOffset),
                         0.0, 1.0);
  color = clip (inPos, color);
}

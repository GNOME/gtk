#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in flat Rect inRect;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in flat uint inTexId;
layout(location = 4) in flat vec4 inColor;

layout(location = 0) out vec4 color;

void main()
{
  float alpha = inColor.a * rect_coverage (inRect, inPos);
  alpha *= texture(get_sampler (inTexId), inTexCoord).a;
  color = clip_scaled (inPos, vec4(inColor.rgb, 1) * alpha);
}

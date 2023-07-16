#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in Rect inRect;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inTexId;

layout(location = 0) out vec4 color;

void main()
{
  float alpha = rect_coverage (inRect, inPos);
  color = clip_scaled (inPos, texture (get_sampler (inTexId), inTexCoord) * alpha);
}

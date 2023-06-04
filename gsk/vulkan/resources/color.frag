#version 420 core

#include "clip.frag.glsl"
#include "rect.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in Rect inRect;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 color;

void main()
{
  float alpha = inColor.a * rect_coverage (inRect, inPos);
  color = clip_scaled (inPos, vec4(inColor.rgb, 1) * alpha);
}

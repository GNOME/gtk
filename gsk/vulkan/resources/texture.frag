#version 420 core

#include "clip.frag.glsl"
#include "rect.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in Rect inRect;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform sampler2D inTexture;

layout(location = 0) out vec4 color;

void main()
{
  float alpha = rect_coverage (inRect, inPos);
  color = clip_scaled (inPos, texture (inTexture, inTexCoord) * alpha);
}

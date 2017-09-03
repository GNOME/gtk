#version 420 core

#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in float inRadius;

layout(location = 0) out vec4 color;

void main()
{
  color = clip (inPos, vec4(1, 0, 0, 0));
}

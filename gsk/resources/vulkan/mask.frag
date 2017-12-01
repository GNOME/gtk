#version 420 core

#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(set = 0, binding = 0) uniform sampler2D inTexture;

layout(location = 0) out vec4 color;

void main()
{
  color = clip (inPos, vec4(inColor.rgb * inColor.a, inColor.a) * texture(inTexture, inTexCoord).a);
}

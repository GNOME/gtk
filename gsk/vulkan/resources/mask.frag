#version 420 core

#include "common.frag.glsl"
#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;
layout(location = 3) flat in uvec2 inTexId;

layout(location = 0) out vec4 color;

void main()
{
  color = clip (inPos, vec4(inColor.rgb * inColor.a, inColor.a) * texture(get_sampler (inTexId), inTexCoord).a);
}

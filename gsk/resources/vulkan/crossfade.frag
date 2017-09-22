#version 420 core

#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in float inProgress;

layout(set = 0, binding = 0) uniform sampler2D startTexture;
layout(set = 1, binding = 0) uniform sampler2D endTexture;

layout(location = 0) out vec4 color;

void main()
{
  vec4 start = texture (startTexture, inTexCoord);
  vec4 end = texture (endTexture, inTexCoord);

  color = clip (inPos, mix (start, end, inProgress));
}

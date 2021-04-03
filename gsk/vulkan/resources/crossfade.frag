#version 420 core

#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inStartTexCoord;
layout(location = 2) in vec2 inEndTexCoord;
layout(location = 3) in float inProgress;

layout(set = 0, binding = 0) uniform sampler2D startTexture;
layout(set = 1, binding = 0) uniform sampler2D endTexture;

layout(location = 0) out vec4 color;

void main()
{
  vec4 start = texture (startTexture, inStartTexCoord);
  vec4 end = texture (endTexture, inEndTexCoord);

  color = clip (inPos, mix (start, end, inProgress));
}

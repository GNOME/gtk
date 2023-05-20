#version 420 core

#include "common.frag.glsl"
#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inStartTexCoord;
layout(location = 2) in vec2 inEndTexCoord;
layout(location = 3) flat in uint inStartTexId;
layout(location = 4) flat in uint inEndTexId;
layout(location = 5) in float inProgress;

layout(location = 0) out vec4 color;

void main()
{
  vec4 start = texture (textures[inStartTexId], inStartTexCoord);
  vec4 end = texture (textures[inEndTexId], inEndTexCoord);

  color = clip (inPos, mix (start, end, inProgress));
}

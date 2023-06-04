#version 420 core

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inStartRect;
layout(location = 2) in vec4 inEndRect;
layout(location = 3) in vec4 inStartTexRect;
layout(location = 4) in vec4 inEndTexRect;
layout(location = 5) in uvec2 inStartTexId;
layout(location = 6) in uvec2 inEndTexId;
layout(location = 7) in float inProgress;

layout(location = 0) out vec2 outPos;
layout(location = 1) flat out Rect outStartRect;
layout(location = 2) flat out Rect outEndRect;
layout(location = 3) out vec2 outStartTexCoord;
layout(location = 4) out vec2 outEndTexCoord;
layout(location = 5) flat out uvec2 outStartTexId;
layout(location = 6) flat out uvec2 outEndTexId;
layout(location = 7) flat out float outProgress;

void main() {
  Rect r = rect_from_gsk (inRect);
  vec2 pos = set_position_from_rect (r);

  outPos = pos;
  outStartRect = rect_from_gsk (inStartRect);
  outEndRect = rect_from_gsk (inEndRect);
  outStartTexCoord = scale_tex_coord (pos, r, inStartTexRect);
  outEndTexCoord = scale_tex_coord (pos, r, inEndTexRect);
  outStartTexId = inStartTexId;
  outEndTexId = inEndTexId;
  outProgress = inProgress;
}

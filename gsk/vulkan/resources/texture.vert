#version 420 core

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inTexRect;
layout(location = 2) in uvec2 inTexId;

layout(location = 0) out vec2 outPos;
layout(location = 1) out flat Rect outRect;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out flat uvec2 outTexId;

void main() {
  Rect r = rect_from_gsk (inRect);
  vec2 pos = set_position_from_rect (r);

  outPos = pos;
  outRect = r;
  outTexCoord = scale_tex_coord (pos, r, inTexRect);
  outTexId = inTexId;
}

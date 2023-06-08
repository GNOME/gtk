#version 450

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec2 outPos;
layout(location = 1) out flat Rect outRect;
layout(location = 2) out flat vec4 outColor;

void main() {
  Rect r = rect_from_gsk (inRect);
  outPos = set_position_from_rect (r);
  outRect = r;
  outColor = inColor;
}

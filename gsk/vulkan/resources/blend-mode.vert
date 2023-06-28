#version 450

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inTopRect;
layout(location = 2) in vec4 inBottomRect;
layout(location = 3) in vec4 inTopTexRect;
layout(location = 4) in vec4 inBottomTexRect;
layout(location = 5) in uint inTopTexId;
layout(location = 6) in uint inBottomTexId;
layout(location = 7) in uint inBlendMode;

layout(location = 0) out vec2 outPos;
layout(location = 1) flat out Rect outTopRect;
layout(location = 2) flat out Rect outBottomRect;
layout(location = 3) out vec2 outTopTexCoord;
layout(location = 4) out vec2 outBottomTexCoord;
layout(location = 5) flat out uint outTopTexId;
layout(location = 6) flat out uint outBottomTexId;
layout(location = 7) flat out uint outBlendMode;

void main() {
  Rect r = rect_from_gsk (inRect);
  vec2 pos = set_position_from_rect (r);

  outPos = pos;
  outTopRect = rect_from_gsk (inTopRect);
  outBottomRect = rect_from_gsk (inBottomRect);
  outTopTexCoord = scale_tex_coord (pos, r, inTopTexRect);
  outBottomTexCoord = scale_tex_coord (pos, r, inBottomTexRect);
  outTopTexId = inTopTexId;
  outBottomTexId = inBottomTexId;
  outBlendMode = inBlendMode;
}

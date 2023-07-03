#version 450

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inTexRect;
layout(location = 2) in vec4 inColor;
layout(location = 3) in uint inTexId;

layout(location = 0) out vec2 outPos;
layout(location = 1) out flat Rect outRect;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out flat uint outTexId;
layout(location = 4) out flat vec4 outColor;

void main() {
  Rect r = rect_from_gsk (inRect);
  vec2 pos = set_position_from_rect (r);

  outPos = pos;
  outRect = r;
  outTexCoord = scale_tex_coord (pos, r, inTexRect);
  outTexId = inTexId;
  outColor = inColor;
}

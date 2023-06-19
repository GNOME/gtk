#version 450

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec2 inStart;
layout(location = 2) in vec2 inEnd;
layout(location = 3) in int inRepeating;
layout(location = 4) in int inStopOffset;
layout(location = 5) in int inStopCount;

layout(location = 0) out vec2 outPos;
layout(location = 1) out flat Rect outRect;
layout(location = 2) out float outGradientPos;
layout(location = 3) out flat int outRepeating;
layout(location = 4) out flat int outStopOffset;
layout(location = 5) out flat int outStopCount;

float
get_gradient_pos (vec2 pos)
{
  pos = pos - inStart * push.scale;
  vec2 grad = (inEnd - inStart) * push.scale;

  return dot (pos, grad) / dot (grad, grad);
}

void main() {
  Rect r = rect_from_gsk (inRect);
  vec2 pos = set_position_from_rect (r);
  outPos = pos;
  outRect = r;
  outGradientPos = get_gradient_pos (pos);
  outRepeating = inRepeating;
  outStopOffset = inStopOffset;
  outStopCount = inStopCount;
}

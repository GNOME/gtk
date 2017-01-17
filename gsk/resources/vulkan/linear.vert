#version 420 core

#include "clip.vert.glsl"

struct ColorStop {
  float offset;
  vec4 color;
};

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec2 inStart;
layout(location = 2) in vec2 inEnd;
layout(location = 3) in int inRepeating;
layout(location = 4) in int inStopCount;
layout(location = 5) in vec4 inOffsets0;
layout(location = 6) in vec4 inOffsets1;
layout(location = 7) in vec4 inColors0;
layout(location = 8) in vec4 inColors1;
layout(location = 9) in vec4 inColors2;
layout(location = 10) in vec4 inColors3;
layout(location = 11) in vec4 inColors4;
layout(location = 12) in vec4 inColors5;
layout(location = 13) in vec4 inColors6;
layout(location = 14) in vec4 inColors7;

layout(location = 0) out vec2 outPos;
layout(location = 1) out float outGradientPos;
layout(location = 2) out flat int outRepeating;
layout(location = 3) out flat int outStopCount;
layout(location = 4) out flat ColorStop outStops[8];

out gl_PerVertex {
  vec4 gl_Position;
};

vec2 offsets[6] = { vec2(0.0, 0.0),
                    vec2(1.0, 0.0),
                    vec2(0.0, 1.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 0.0),
                    vec2(1.0, 1.0) };

float
get_gradient_pos (vec2 pos)
{
  pos = pos - inStart;
  vec2 grad = inEnd - inStart;

  return dot (pos, grad) / dot (grad, grad);
}

void main() {
  vec4 rect = clip (inRect);
  vec2 pos = rect.xy + rect.zw * offsets[gl_VertexIndex];
  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);
  outPos = pos;
  outGradientPos = get_gradient_pos (pos);
  outRepeating = inRepeating;
  outStopCount = inStopCount;
  outStops[0].offset = inOffsets0[0];
  outStops[0].color = inColors0 * vec4(inColors0.aaa, 1.0);
  outStops[1].offset = inOffsets0[1];
  outStops[1].color = inColors1 * vec4(inColors1.aaa, 1.0);
  outStops[2].offset = inOffsets0[2];
  outStops[2].color = inColors2 * vec4(inColors2.aaa, 1.0);
  outStops[3].offset = inOffsets0[3];
  outStops[3].color = inColors3 * vec4(inColors3.aaa, 1.0);
  outStops[4].offset = inOffsets1[0];
  outStops[4].color = inColors4 * vec4(inColors4.aaa, 1.0);
  outStops[5].offset = inOffsets1[1];
  outStops[5].color = inColors5 * vec4(inColors5.aaa, 1.0);
  outStops[6].offset = inOffsets1[2];
  outStops[6].color = inColors6 * vec4(inColors6.aaa, 1.0);
  outStops[7].offset = inOffsets1[3];
  outStops[7].color = inColors7 * vec4(inColors7.aaa, 1.0);
}

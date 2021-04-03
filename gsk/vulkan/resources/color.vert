#version 420 core

#include "clip.vert.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec2 outPos;
layout(location = 1) out flat vec4 outColor;

vec2 offsets[6] = { vec2(0.0, 0.0),
                    vec2(1.0, 0.0),
                    vec2(0.0, 1.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 0.0),
                    vec2(1.0, 1.0) };

void main() {
  vec4 rect = clip (inRect);

  vec2 pos = rect.xy + rect.zw * offsets[gl_VertexIndex];
  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);
  outPos = pos;
  outColor = inColor;
}

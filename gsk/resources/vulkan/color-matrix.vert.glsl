#version 420 core

#include "constants.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inTexRect;
layout(location = 2) in mat4 inColorMatrix;
layout(location = 6) in vec4 inColorOffset;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out flat mat4 outColorMatrix;
layout(location = 5) out flat vec4 outColorOffset;

out gl_PerVertex {
  vec4 gl_Position;
};

vec2 offsets[6] = { vec2(0.0, 0.0),
                    vec2(1.0, 0.0),
                    vec2(0.0, 1.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 0.0),
                    vec2(1.0, 1.0) };

void main() {
  vec2 pos = inRect.xy + inRect.zw * offsets[gl_VertexIndex];
  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);

  outTexCoord = inTexRect.xy + inTexRect.zw * offsets[gl_VertexIndex];
  outColorMatrix = inColorMatrix;
  outColorOffset = inColorOffset;
}

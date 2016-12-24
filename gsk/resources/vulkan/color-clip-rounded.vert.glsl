#version 420 core

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 clip_bounds;
    vec4 clip_widths;
    vec4 clip_heights;
} push;

layout(location = 0) out vec2 outPos;
layout(location = 1) out flat vec4 outColor;
layout(location = 2) out flat vec4 outClipBounds;
layout(location = 3) out flat vec4 outClipWidths;

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
  outPos = pos;
  outColor = inColor;
  outClipBounds = push.clip_bounds;
  outClipWidths = push.clip_widths;
}

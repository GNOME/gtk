#version 420 core

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 clip_bounds;
    vec4 clip_widths;
    vec4 clip_heights;
} push;

layout(location = 0) out vec2 outTexCoord;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() {
  gl_Position = push.mvp * vec4 (inPosition, 0.0, 1.0);
  outTexCoord = inTexCoord;
}

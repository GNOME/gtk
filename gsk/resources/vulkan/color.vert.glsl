#version 420 core

layout(location = 0) in vec2 inPosition;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} push;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() {
  gl_Position = push.mvp * vec4 (inPosition, 0.0, 1.0);
}

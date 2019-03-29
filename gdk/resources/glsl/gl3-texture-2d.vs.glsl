#version 330

in vec2 position;
in vec2 uv;

out vec2 vUv;

void main() {
  gl_Position = vec4(position, 0, 1);
  vUv = uv;
}

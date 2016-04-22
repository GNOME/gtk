#version 150

uniform sampler2D map;

in vec2 position;
in vec2 uv;

out vec2 vUv;

void main() {
  gl_Position = vec4(position, 0, 1);
  vUv = uv;
}

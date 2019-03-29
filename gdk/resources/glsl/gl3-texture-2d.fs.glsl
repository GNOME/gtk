#version 330

in vec2 vUv;

out vec4 vertexColor;

uniform sampler2D map;

void main() {
  vertexColor = texture (map, vUv);
}

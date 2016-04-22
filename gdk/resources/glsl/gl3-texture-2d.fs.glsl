#version 150

in vec2 vUv;

out vec4 vertexColor;

uniform sampler2D map;

void main() {
  vertexColor = texture2D (map, vUv);
}

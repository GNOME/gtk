#version 150

smooth in vec2 vUv;

out vec4 outputColor;

uniform mat4 mvp;
uniform sampler2D map;
uniform float alpha;

void main() {
  outputColor = texture2D(map, vUv) * vec4(alpha);
}

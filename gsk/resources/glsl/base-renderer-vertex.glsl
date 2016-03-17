#version 150

uniform mat4 mvp;
uniform sampler2D map;
uniform float alpha;

in vec2 position;
in vec2 uv;

smooth out vec2 vUv;

void main() {
  gl_Position = mvp * vec4(position, 0.0, 1.0);

  vUv = vec2(uv.x, 1 - uv.y);
}

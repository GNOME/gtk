#version 110

varying vec2 vUv;

uniform sampler2D map;

void main() {
  gl_FragColor = texture2D (map, vUv);
}

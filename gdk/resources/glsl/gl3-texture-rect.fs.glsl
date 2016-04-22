#version 150

varying vec2 vUv;

uniform sampler2DRect map;

void main() {
  gl_FragColor = texture2DRect (map, vUv);
}

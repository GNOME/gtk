precision mediump float;

uniform sampler2D map;

varying highp vec2 vUv;

void main() {
  gl_FragColor = texture2D(map, vUv);
}

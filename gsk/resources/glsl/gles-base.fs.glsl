precision mediump float;

uniform mat4 mvp;
uniform sampler2D map;
uniform float alpha;

varying vec2 vUv;

void main() {
  gl_FragColor = texture2D(map, vUv) * vec4(alpha);
}

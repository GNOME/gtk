precision mediump float;

uniform sampler2D map;

varying highp vec2 vUv;

void main() {
  vec4 color = texture2D(map, vUv);

  /* Flip R and B around to match the Cairo convention */
  gl_FragColor = vec4(color.z, color.y, color.x, color.w);
}

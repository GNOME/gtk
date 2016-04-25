precision mediump float;

uniform sampler2D map;
uniform int flipColors;

varying highp vec2 vUv;

void main() {
  vec4 color = texture2D(map, vUv);

  /* Flip R and B around to match the Cairo convention, if required */
  if (flipColors == 1)
    gl_FragColor = vec4(color.z, color.y, color.x, color.w);
  else
    gl_FragColor = color;
}

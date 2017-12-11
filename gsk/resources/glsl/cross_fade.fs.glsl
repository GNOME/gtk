
uniform float u_progress;
uniform sampler2D u_source2;

void main() {
  vec4 source1 = Texture(u_source, vUv);  // start child
  vec4 source2 = Texture(u_source2, vUv); // end child

  float p = u_progress;
  vec4 color = ((1 - p) * source1) + (p * source2);
  setOutputColor(color);
}

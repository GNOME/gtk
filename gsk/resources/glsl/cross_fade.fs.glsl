
uniform float u_progress;
uniform sampler2D u_source2;

void main() {
  vec4 source1 = Texture(u_source, vUv);  // start child
  vec4 source2 = Texture(u_source2, vUv); // end child

  float p_start = (1.0 - u_progress) * u_alpha;
  float p_end = u_progress * u_alpha;
  vec4 color = (p_start * source1) + (p_end * source2);
  setOutputColor(color);
}

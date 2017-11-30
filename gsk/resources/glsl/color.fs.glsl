uniform vec4 u_color;

void main() {
  vec4 color = u_color;

  // Pre-multiply alpha
  color.rgb *= color.a;
  setOutputColor(color * u_alpha);
}

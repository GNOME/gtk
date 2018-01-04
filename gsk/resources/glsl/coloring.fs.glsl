uniform vec4 u_color;

void main() {
  vec4 diffuse = Texture(u_source, vUv);
  vec4 color = u_color;

  // pre-multiply
  color.rgb *= color.a;

  // u_source is drawn using cairo, so already pre-multiplied.

  setOutputColor(color * diffuse * u_alpha);
}

uniform vec4 u_color;

void main() {
  vec4 diffuse = Texture(u_source, vUv);
  vec4 color = u_color;

  // pre-multiply
  color.rgb *= color.a;

  // u_source is drawn using cairo, so already pre-multiplied.
  color = vec4(color.rgb * diffuse.a * u_alpha,
               color.a * diffuse.a * u_alpha);

  setOutputColor(color);
}

uniform vec4 u_color;

void main() {
  vec4 diffuse = Texture(u_source, vUv);
  vec4 color = u_color;

  // pre-multiply
  color.rgb *= color.a;

  color = vec4(u_color.rgb * diffuse.a, diffuse.a * color.a);

  setOutputColor(color);
}

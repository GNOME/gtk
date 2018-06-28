void main() {
  vec4 diffuse = Texture(u_source, vUv);

  setOutputColor(diffuse * u_alpha);
}

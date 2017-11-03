void main() {
  vec4 diffuse = Texture(uSource, vUv);

  setOutputColor(diffuse * uAlpha);
}

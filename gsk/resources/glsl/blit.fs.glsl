void main() {
  vec4 diffuse = Texture(uSource, vUv);

  setOutputColor(vec4(diffuse.xyz, diffuse.a * uAlpha));
}

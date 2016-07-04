void main() {
  vec4 diffuse = Texture(map, vUv);

  setOutputColor(vec4(diffuse.xyz, diffuse.a * alpha));
}

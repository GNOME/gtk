
uniform vec4 uColor;

void main() {
  vec4 diffuse = Texture(uSource, vUv);
  vec4 color = uColor;

  // pre-multiply
  color.rgb *= color.a;

  setOutputColor((diffuse * color) * uAlpha);
}

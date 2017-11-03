uniform vec4 uColor;

void main() {
  vec4 color = uColor;

  // Pre-multiply alpha
  color.rgb *= color.a;
  setOutputColor(color * uAlpha);
}

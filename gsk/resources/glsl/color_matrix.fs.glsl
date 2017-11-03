uniform mat4 uColorMatrix;
uniform vec4 uColorOffset;

void main() {
  vec4 diffuse = Texture(uSource, vUv);
  vec4 color;

  color = diffuse;

  // Un-premultilpy
  if (color.a != 0.0)
    color.rgb /= color.a;

  color = uColorMatrix * diffuse + uColorOffset;
  color = clamp(color, 0.0f, 1.0f);

  color.rgb *= color.a;

  setOutputColor(color * uAlpha);
}

uniform mat4 u_color_matrix;
uniform vec4 u_color_offset;

void main() {
  vec4 diffuse = Texture(u_source, vUv);
  vec4 color;

  color = diffuse;

  // Un-premultilpy
  if (color.a != 0.0)
    color.rgb /= color.a;

  color = u_color_matrix * diffuse + u_color_offset;
  color = clamp(color, 0.0f, 1.0f);

  color.rgb *= color.a;

  setOutputColor(color * u_alpha);
}

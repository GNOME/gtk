// VERTEX_SHADER:
void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);
}

// FRAGMENT_SHADER:
uniform vec4 u_color;

void main() {
  vec4 color = u_color;

  // Pre-multiply alpha
  color.rgb *= color.a;
  setOutputColor(color * u_alpha);
}


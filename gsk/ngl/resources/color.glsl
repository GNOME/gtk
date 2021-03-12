// VERTEX_SHADER:
_OUT_ vec4 final_color;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  final_color = gsk_premultiply(aColor) * u_alpha;
}

// FRAGMENT_SHADER:
_IN_ vec4 final_color;

void main() {
  gskSetOutputColor(final_color);
}


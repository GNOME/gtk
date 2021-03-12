// VERTEX_SHADER:
_OUT_ vec4 final_color;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);

  final_color = gsk_premultiply(aColor) * u_alpha;
}

// FRAGMENT_SHADER:

_IN_ vec4 final_color;

void main() {
  vec4 diffuse = GskTexture(u_source, vUv);

  gskSetOutputColor(final_color * diffuse.a);
}


// VERTEX_SHADER:
// linearize_premultiply.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// linearize_premultiply.glsl

void main() {
  vec4 diffuse = GskTexture(u_source, vUv);

  diffuse = gsk_srgb_to_linear(diffuse);
  diffuse = gsk_premultiply(diffuse);

  gskSetOutputColor(diffuse);
}

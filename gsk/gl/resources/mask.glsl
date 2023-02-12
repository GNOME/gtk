// VERTEX_SHADER:
// mask.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// mask.glsl

uniform sampler2D u_mask;

void main() {
  vec4 source = GskTexture(u_source, vUv);
  vec4 mask = GskTexture(u_mask, vUv);
  gskSetOutputColor(vec4 (source * mask.a));
}

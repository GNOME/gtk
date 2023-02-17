// VERTEX_SHADER:
// mask.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// mask.glsl

uniform int u_mode;
uniform sampler2D u_mask;

void main() {
  vec4 source = GskTexture(u_source, vUv);
  vec4 mask = GskTexture(u_mask, vUv);
  float mask_value;

  if (u_mode == 0)
    mask_value = mask.a;
  else if (u_mode == 1)
    mask_value = 1.0 - mask.a;
  else if (u_mode == 2)
    mask_value = (0.2126 * mask.r + 0.7152 * mask.g + 0.0722 * mask.b) * mask.a;
  else if (u_mode == 3)
    mask_value = 1.0 - (0.2126 * mask.r + 0.7152 * mask.g + 0.0722 * mask.b) * mask.a;
  else
    mask_value = 0.0;

  gskSetOutputColor(vec4 (source * mask_value));
}

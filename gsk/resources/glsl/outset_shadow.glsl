// VERTEX_SHADER:
void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
uniform vec4[3] u_outline_rect;

void main() {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect outline = create_rect(u_outline_rect);
  vec4 color = Texture(u_source, vUv);
  color = color * (1.0 -  clamp(rounded_rect_coverage(outline, f.xy), 0.0, 1.0));
  setOutputColor(color * u_alpha);
}

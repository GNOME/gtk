// VERTEX_SHADER:
void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
uniform float u_spread;
uniform vec4 u_color;
uniform vec2 u_offset;
uniform RoundedRect u_outline_rect;

void main() {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect inside = rounded_rect_shrink(u_outline_rect, vec4(u_spread));
  vec2 offset = vec2(u_offset.x, - u_offset.y);
  vec4 color = vec4(u_color.rgb * u_color.a, u_color.a);
  color = color * clamp (rounded_rect_coverage (u_outline_rect, f.xy) -
                         rounded_rect_coverage (inside, f.xy - offset),
                         0.0, 1.0);
  setOutputColor(color * u_alpha);
}

uniform float u_spread;
uniform vec4 u_color;
uniform vec2 u_offset;
uniform RoundedRect u_outline_rect;

void main() {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect outline = rounded_rect_shrink(u_outline_rect, vec4(- u_spread));

  vec2 offset = vec2(u_offset.x, - u_offset.y);
  vec4 color = vec4(u_color.rgb * u_color.a, u_color.a);
  color = color * clamp (rounded_rect_coverage (outline, f.xy - offset) -
                         rounded_rect_coverage (u_outline_rect, f.xy),
                         0.0, 1.0);
  setOutputColor(color * u_alpha);
}

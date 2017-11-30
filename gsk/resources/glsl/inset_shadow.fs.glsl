uniform float u_spread;
uniform float u_blur_radius;
uniform vec4 u_color;
uniform vec2 u_offset;
uniform vec4 u_outline;
uniform vec4 u_corner_widths;
uniform vec4 u_corner_heights;


void main() {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect outline = RoundedRect(vec4(u_outline.xy, u_outline.xy + u_outline.zw),
                                    u_corner_widths, u_corner_heights);
  RoundedRect inside = rounded_rect_shrink(outline, vec4(u_spread));



  vec4 color = vec4(u_color.rgb * u_color.a, u_color.a);
  color = color * clamp (rounded_rect_coverage (outline, f.xy) -
                         rounded_rect_coverage (inside, f.xy + u_offset),
                         0.0, 1.0);
  setOutputColor(color);
}

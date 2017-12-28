uniform float u_spread;
uniform float u_blur_radius;
uniform vec4 u_color;
uniform vec2 u_offset;
uniform vec4 u_outline;
uniform vec4 u_corner_widths = vec4(0, 0, 0, 0);
uniform vec4 u_corner_heights = vec4(0, 0, 0, 0);

void main() {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect outline = RoundedRect(vec4(u_outline.xy, u_outline.xy + u_outline.zw), u_corner_widths, u_corner_heights);

  vec4 color = Texture(u_source, vUv);
  color = color * (1 -  clamp(rounded_rect_coverage (outline, f.xy), 0, 1));
  setOutputColor(color * u_alpha);
}

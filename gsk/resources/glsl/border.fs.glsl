uniform vec4 u_color;
uniform vec4 u_widths;

uniform vec4 u_outline;
uniform vec4 u_corner_widths;
uniform vec4 u_corner_heights;

void main() {
  vec4 bounds = u_outline;

  bounds.z = bounds.x + bounds.z;
  bounds.w = bounds.y + bounds.w;

  vec4 f = gl_FragCoord;
  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect routside = RoundedRect (bounds, u_corner_widths, u_corner_heights);
  RoundedRect rinside = rounded_rect_shrink (routside, u_widths);

  float alpha = clamp (rounded_rect_coverage (routside, f.xy) -
                       rounded_rect_coverage (rinside, f.xy),
                       0.0, 1.0);

  /* Pre-multiply */
  vec4 color = u_color;
  color.rgb *= color.a;

  setOutputColor (color * alpha * u_alpha);
}

uniform vec4 u_color = vec4(1, 0, 1, 1);
uniform vec4 u_widths;

// For border we abuse, ehm, re-use, the global clip
// as rounded rect and shrink it by u_widths
void main() {
  vec4 bounds = u_clip;
  vec4 f = gl_FragCoord;


  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  bounds.z = bounds.x + bounds.z;
  bounds.w = bounds.y + bounds.w;

  vec4 corner_widths = max (u_clip_corner_widths, u_widths.wyyw);
  vec4 corner_heights = max (u_clip_corner_heights, u_widths.xxzz);

  RoundedRect routside = RoundedRect (bounds, corner_widths, corner_heights);
  RoundedRect rinside = rounded_rect_shrink (routside, u_widths);

  float alpha = clamp (rounded_rect_coverage (routside, f.xy) -
                       rounded_rect_coverage (rinside, f.xy),
                       0.0, 1.0);

  /* Pre-multiply */
  vec4 color = u_color;
  color.rgb *= color.a;

  setOutputColor (color * alpha * u_alpha);
}

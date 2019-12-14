// VERTEX_SHADER:

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
uniform vec4 u_color;
uniform vec4 u_widths;
uniform RoundedRect u_outline_rect;

void main() {
  vec4 f = gl_FragCoord;
  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect rinside = rounded_rect_shrink (u_outline_rect, u_widths);

  float alpha = clamp (rounded_rect_coverage (u_outline_rect, f.xy) -
                       rounded_rect_coverage (rinside, f.xy),
                       0.0, 1.0);

  /* Pre-multiply */
  vec4 color = u_color;
  color.rgb *= color.a;

  setOutputColor (color * alpha * u_alpha);
}

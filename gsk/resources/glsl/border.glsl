// VERTEX_SHADER:
uniform vec4 u_color;

_OUT_ vec4 final_color;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  final_color = u_color;
  final_color.rgb *= final_color.a; // pre-multiply
  final_color *= u_alpha;
}

// FRAGMENT_SHADER:
uniform vec4 u_widths;
uniform vec4[3] u_outline_rect;

_IN_ vec4 final_color;

void main() {
  vec4 f = gl_FragCoord;
  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect outside = create_rect(u_outline_rect);
  RoundedRect inside = rounded_rect_shrink (outside, u_widths);

  float alpha = clamp (rounded_rect_coverage (outside, f.xy) -
                       rounded_rect_coverage (inside, f.xy),
                       0.0, 1.0);

  setOutputColor(final_color * alpha);
}

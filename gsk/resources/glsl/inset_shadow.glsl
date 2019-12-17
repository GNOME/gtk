// VERTEX_SHADER:
uniform vec4 u_color;

_OUT_ vec4 final_color;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  final_color = u_color;
  final_color.rgb *= final_color.a;
  final_color *= u_alpha;
}

// FRAGMENT_SHADER:
uniform float u_spread;
uniform vec2 u_offset;
uniform vec4[3] u_outline_rect;

_IN_ vec4 final_color;

void main() {
  vec4 f = gl_FragCoord;
  vec4 color;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect outside = create_rect(u_outline_rect);
  RoundedRect inside = rounded_rect_shrink(outside, vec4(u_spread));
  color = final_color * clamp (rounded_rect_coverage (outside, f.xy) -
                               rounded_rect_coverage (inside, f.xy - u_offset),
                               0.0, 1.0);
  setOutputColor(color);
}

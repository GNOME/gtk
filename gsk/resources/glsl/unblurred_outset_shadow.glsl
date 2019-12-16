// VERTEX_SHADER:
void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
uniform float u_spread;
uniform vec4 u_color;
uniform vec2 u_offset;
uniform vec4[3] u_outline_rect;

void main() {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;


  RoundedRect inside = create_rect(u_outline_rect);
  RoundedRect outside = rounded_rect_shrink(inside, vec4(- u_spread));

  vec2 offset = vec2(u_offset.x, - u_offset.y);
  vec4 color = vec4(u_color.rgb * u_color.a, u_color.a);
  color = color * clamp (rounded_rect_coverage (outside, f.xy - offset) -
                         rounded_rect_coverage (inside, f.xy),
                         0.0, 1.0);
  setOutputColor(color * u_alpha);
}

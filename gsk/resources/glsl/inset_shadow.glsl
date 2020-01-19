// VERTEX_SHADER:
uniform vec4 u_color;
uniform float u_spread;
uniform vec2 u_offset;
uniform vec4[3] u_outline_rect;

_OUT_ vec4 final_color;
_OUT_ _ROUNDED_RECT_UNIFORM_ transformed_outside_outline;
_OUT_ _ROUNDED_RECT_UNIFORM_ transformed_inside_outline;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  final_color = u_color;
  final_color.rgb *= final_color.a;
  final_color *= u_alpha;

  RoundedRect outside = create_rect(u_outline_rect);
  RoundedRect inside = rounded_rect_shrink(outside, vec4(u_spread));

  rounded_rect_offset(inside, u_offset);

  rounded_rect_transform(outside, u_modelview);
  rounded_rect_transform(inside, u_modelview);

  rounded_rect_encode(outside, transformed_outside_outline);
  rounded_rect_encode(inside, transformed_inside_outline);
}

// FRAGMENT_SHADER:
_IN_ vec4 final_color;
_IN_ _ROUNDED_RECT_UNIFORM_ transformed_outside_outline;
_IN_ _ROUNDED_RECT_UNIFORM_ transformed_inside_outline;

void main() {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  float alpha = clamp (rounded_rect_coverage(decode_rect(transformed_outside_outline), f.xy) -
                       rounded_rect_coverage(decode_rect(transformed_inside_outline), f.xy),
                       0.0, 1.0);

  setOutputColor(final_color * alpha);
}

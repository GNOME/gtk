// VERTEX_SHADER:
uniform vec4 u_color;
uniform vec4[3] u_outline_rect;

_OUT_ vec4 final_color;
_OUT_ _ROUNDED_RECT_UNIFORM_ transformed_outline;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);

  final_color = u_color;
  // pre-multiply
  final_color.rgb *= final_color.a;
  final_color *= u_alpha;

  RoundedRect outline = create_rect(u_outline_rect);
  rounded_rect_transform(outline, u_modelview);
  rounded_rect_encode(outline, transformed_outline);
}

// FRAGMENT_SHADER:
_IN_ vec4 final_color;
_IN_ _ROUNDED_RECT_UNIFORM_ transformed_outline;

void main() {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  float alpha = Texture(u_source, vUv).a;
  alpha *= (1.0 -  clamp(rounded_rect_coverage(decode_rect(transformed_outline), f.xy), 0.0, 1.0));

  vec4 color = final_color * alpha;

  setOutputColor(color);
}

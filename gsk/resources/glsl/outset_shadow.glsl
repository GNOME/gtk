// VERTEX_SHADER:
uniform vec4 u_color;

_OUT_ vec4 final_color;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);

  final_color = u_color;
  // pre-multiply
  final_color.rgb *= final_color.a;
  final_color *= u_alpha;
}

// FRAGMENT_SHADER:
uniform vec4[3] u_outline_rect;

_IN_ vec4 final_color;

void main() {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  RoundedRect outline = create_rect(u_outline_rect);

  float alpha = Texture(u_source, vUv).a;
  alpha *= (1.0 -  clamp(rounded_rect_coverage(outline, f.xy), 0.0, 1.0));

  vec4 color = final_color * alpha;

  setOutputColor(color);
}

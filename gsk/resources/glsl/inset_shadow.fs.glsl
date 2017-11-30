uniform float uSpread;
uniform float uBlurRadius;
uniform vec4 uColor;
uniform vec2 uD;
uniform vec4 uOutline;
uniform vec4 uOutlineCornerWidths;
uniform vec4 uOutlineCornerHeights;


void main() {
  vec4 f = gl_FragCoord;

  f.x += uViewport.x;
  f.y = (uViewport.y + uViewport.w) - f.y;

  RoundedRect outline = RoundedRect(vec4(uOutline.xy, uOutline.xy + uOutline.zw),
                                    uOutlineCornerWidths, uOutlineCornerHeights);
  RoundedRect inside = rounded_rect_shrink(outline, vec4(uSpread));

  vec4 color = vec4(uColor.rgb * uColor.a, uColor.a);
  color = color * clamp (rounded_rect_coverage (outline, f.xy) -
                         rounded_rect_coverage (inside, f.xy - uD),
                         0.0, 1.0);
  setOutputColor(color);

  /*setOutputColor(vec4(1, 0, 0, 1) * rounded_rect_coverage (outline, f.xy));*/
  /*setOutputColor(vec4(0, 0, 1, 1) * rounded_rect_coverage (inside, f.xy));*/
}

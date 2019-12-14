// VERTEX_SHADER:
void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
uniform vec4 u_color_stops[8];
uniform float u_color_offsets[8];
uniform int u_num_color_stops;
uniform vec2 u_start_point;
uniform vec2 u_end_point;

vec4 fragCoord() {
  vec4 f = gl_FragCoord;
  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;
  return f;
}

void main() {
  vec2 startPoint = (u_modelview * vec4(u_start_point, 0, 1)).xy;
  vec2 endPoint   = (u_modelview * vec4(u_end_point,   0, 1)).xy;
  float maxDist   = length(endPoint - startPoint);

  // Position relative to startPoint
  vec2 pos = fragCoord().xy - startPoint;

  // Gradient direction
  vec2 gradient = endPoint - startPoint;
  float gradientLength = length(gradient);

  // Current pixel, projected onto the line between the start point and the end point
  // The projection will be relative to the start point!
  vec2 proj = (dot(gradient, pos) / (gradientLength * gradientLength)) * gradient;

  // Offset of the current pixel
  float offset = length(proj) / maxDist;

  vec4 color = u_color_stops[0];
  for (int i = 1; i < u_num_color_stops; i ++) {
    if (offset >= u_color_offsets[i - 1])  {
      float o = (offset - u_color_offsets[i - 1]) / (u_color_offsets[i] - u_color_offsets[i - 1]);
      color = mix(u_color_stops[i - 1], u_color_stops[i], clamp(o, 0.0, 1.0));
    }
  }

  /* Pre-multiply */
  color.rgb *= color.a;

  setOutputColor(color * u_alpha);
}

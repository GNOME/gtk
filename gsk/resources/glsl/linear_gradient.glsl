// VERTEX_SHADER
uniform vec2 u_start_point;
uniform vec2 u_end_point;

_OUT_ vec2 startPoint;
_OUT_ vec2 endPoint;
_OUT_ float maxDist;
_OUT_ vec2 gradient;
_OUT_ float gradientLength;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  startPoint = (u_modelview * vec4(u_start_point, 0, 1)).xy;
  endPoint   = (u_modelview * vec4(u_end_point,   0, 1)).xy;
  maxDist    = length(endPoint - startPoint);

  // Gradient direction
  gradient = endPoint - startPoint;
  gradientLength = length(gradient);
}

// FRAGMENT_SHADER:
uniform vec4 u_color_stops[8];
uniform float u_color_offsets[8];
uniform int u_num_color_stops;

_IN_ vec2 startPoint;
_IN_ vec2 endPoint;
_IN_ float maxDist;
_IN_ vec2 gradient;
_IN_ float gradientLength;

vec4 fragCoord() {
  vec4 f = gl_FragCoord;
  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;
  return f;
}

void main() {
  // Position relative to startPoint
  vec2 pos = fragCoord().xy - startPoint;

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

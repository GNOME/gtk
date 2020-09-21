// VERTEX_SHADER
uniform vec2 u_start_point;
uniform vec2 u_end_point;
uniform float u_color_stops[6 * 5];
uniform int u_num_color_stops;

_OUT_ vec2 startPoint;
_OUT_ vec2 endPoint;
_OUT_ float maxDist;
_OUT_ vec2 gradient;
_OUT_ float gradientLength;
_OUT_ vec4 color_stops[8];
_OUT_ float color_offsets[8];

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  startPoint = (u_modelview * vec4(u_start_point, 0, 1)).xy;
  endPoint   = (u_modelview * vec4(u_end_point,   0, 1)).xy;
  maxDist    = length(endPoint - startPoint);

  // Gradient direction
  gradient = endPoint - startPoint;
  gradientLength = length(gradient);

  for (int i = 0; i < u_num_color_stops; i ++) {
    color_offsets[i] = u_color_stops[(i * 5) + 0];
    color_stops[i] = premultiply(vec4(u_color_stops[(i * 5) + 1],
                                      u_color_stops[(i * 5) + 2],
                                      u_color_stops[(i * 5) + 3],
                                      u_color_stops[(i * 5) + 4]));
  }
}

// FRAGMENT_SHADER:
#ifdef GSK_LEGACY
uniform int u_num_color_stops;
#else
uniform highp int u_num_color_stops; // Why? Because it works like this.
#endif

_IN_ vec2 startPoint;
_IN_ vec2 endPoint;
_IN_ float maxDist;
_IN_ vec2 gradient;
_IN_ float gradientLength;
_IN_ vec4 color_stops[8];
_IN_ float color_offsets[8];

void main() {
  // Position relative to startPoint
  vec2 pos = get_frag_coord() - startPoint;

  // Current pixel, projected onto the line between the start point and the end point
  // The projection will be relative to the start point!
  vec2 proj = (dot(gradient, pos) / (gradientLength * gradientLength)) * gradient;

  // Offset of the current pixel
  float offset = length(proj) / maxDist;

  vec4 color = color_stops[0];
  for (int i = 1; i < u_num_color_stops; i ++) {
    if (offset >= color_offsets[i - 1])  {
      float o = (offset - color_offsets[i - 1]) / (color_offsets[i] - color_offsets[i - 1]);
      color = mix(color_stops[i - 1], color_stops[i], clamp(o, 0.0, 1.0));
    }
  }

  setOutputColor(color * u_alpha);
}

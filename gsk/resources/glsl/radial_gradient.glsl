// VERTEX_SHADER
uniform float u_start;
uniform float u_end;
uniform float u_color_stops[6 * 5];
uniform int u_num_color_stops;
uniform vec2 u_radius;
uniform vec2 u_center;

_OUT_ vec2 center;
_OUT_ vec4 color_stops[8];
_OUT_ float color_offsets[8];
_OUT_ float start;
_OUT_ float end;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  center = (u_modelview * vec4(u_center, 0, 1)).xy;
  start = u_start;
  end = u_end;

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
uniform highp int u_num_color_stops;
#endif

uniform vec2 u_radius;
uniform float u_end;

_IN_ vec2 center;
_IN_ vec4 color_stops[8];
_IN_ float color_offsets[8];
_IN_ float start;
_IN_ float end;

// The offsets in the color stops are relative to the
// start and end values of the gradient.
float abs_offset(float offset)  {
  return start + ((end - start) * offset);
}

void main() {
  vec2 pixel = get_frag_coord();
  vec2 rel = (center - pixel) / (u_radius);
  float d = sqrt(dot(rel, rel));

  if (d < abs_offset (color_offsets[0])) {
    setOutputColor(color_stops[0] * u_alpha);
    return;
  }

  if (d > end) {
    setOutputColor(color_stops[u_num_color_stops - 1] * u_alpha);
    return;
  }

  vec4 color = vec4(0, 0, 0, 0);
  for (int i = 1; i < u_num_color_stops; i++) {
    float last_offset = abs_offset(color_offsets[i - 1]);
    float this_offset = abs_offset(color_offsets[i]);

    // We have color_stops[i - 1] at last_offset and color_stops[i] at this_offset.
    // We now need to map `d` between those two offsets and simply mix linearly between them
    if (d >= last_offset && d <= this_offset) {
      float f = (d - last_offset) / (this_offset - last_offset);

      color = mix(color_stops[i - 1], color_stops[i], f);
      break;
    }
  }

  setOutputColor(color * u_alpha);
}

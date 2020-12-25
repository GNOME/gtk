// VERTEX_SHADER
uniform vec2 u_center;
uniform float u_rotation;
uniform float u_color_stops[6 * 5];
uniform int u_num_color_stops;

const float PI = 3.1415926535897932384626433832795;

_OUT_ vec2 center;
_OUT_ float rotation;
_OUT_ vec4 color_stops[6];
_OUT_ float color_offsets[6];

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  // The -90 is because conics point to the top by default
  rotation = mod (u_rotation - 90.0, 360.0);
  if (rotation < 0.0)
    rotation += 360.0;
  rotation = PI / 180.0 * rotation;

  center = (u_modelview * vec4(u_center, 0, 1)).xy;

  for (int i = 0; i < u_num_color_stops; i ++) {
    color_offsets[i] = u_color_stops[(i * 5) + 0];
    color_stops[i] = gsk_premultiply(vec4(u_color_stops[(i * 5) + 1],
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

const float PI = 3.1415926535897932384626433832795;

_IN_ vec2 center;
_IN_ float rotation;
_IN_ vec4 color_stops[6];
_IN_ float color_offsets[6];

void main() {
  // Position relative to center
  vec2 pos = gsk_get_frag_coord() - center;

  // direction of point in range [-PI, PI]
  float angle = atan (pos.y, pos.x);
  // rotate, it's now [-2 * PI, PI]
  angle -= rotation;
  // fract() does the modulo here, so now we have progress
  // into the current conic
  float offset = fract (angle / 2.0 / PI + 2.0);

  vec4 color = color_stops[0];
  for (int i = 1; i < u_num_color_stops; i ++) {
    if (offset >= color_offsets[i - 1])  {
      float o = (offset - color_offsets[i - 1]) / (color_offsets[i] - color_offsets[i - 1]);
      color = mix(color_stops[i - 1], color_stops[i], clamp(o, 0.0, 1.0));
    }
  }

  gskSetOutputColor(color * u_alpha);
}

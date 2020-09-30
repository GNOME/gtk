uniform float u_time;

/* 2D -> [0..1] random number generator */
float random(vec2 st) {
  return fract(sin(dot(st.xy,
                       vec2(12.9898,78.233))) *
               43758.5453123);
}

/* Generate a smoothed 2d noise based on random() */
float noise(vec2 v) {
  /* Round point v to integer grid grid */
  vec2 grid_point = floor(v);
  /* Randomize in grid corners */
  float corner1 = random(grid_point);
  float corner2 = random(grid_point + vec2(1, 0));
  float corner3 = random(grid_point + vec2(0, 1));
  float corner4 = random(grid_point + vec2(1, 1));
  /* Interpolate smoothly between grid points */
  vec2 fraction = smoothstep(vec2(0.0), vec2(1.0), fract(v));
  return mix(mix(corner1, corner2, fraction.x),
             mix(corner3, corner4, fraction.x),
             fraction.y);
}

/* fractal brownian motion noice, see https://www.iquilezles.org/www/articles/fbm/fbm.htm */
float fbm(in vec2 x)
{
  const float octaveScale = 1.9;
  const float G = 0.5;
  float f = 1.0;
  float a = 1.0;
  float t = 0.0;
  int numOctaves = 5;
  for (int i = 0; i < numOctaves; i++) {
    t += a*noise(f*x);
    f *= octaveScale;
    a *= G;
  }

  return t;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  vec2 xy = fragCoord / resolution;

  float zoom = 3.0 - sin(u_time*0.5)*0.3;

  // Normalize coord to height of widget
  vec2 p = (vec2 (-resolution.x/2.0 + fragCoord.x, resolution.y - fragCoord.y) / resolution.yy)* zoom;

    // Use recursive incantations of fbm
  float q1 = fbm(p - vec2(0.8, 0.3) * u_time);
  float q2 = fbm(p - vec2(0.5, 1.3) * u_time);
  float r = fbm(2.0*p + vec2(q1,q2) - vec2(0.0, 1.0)*u_time*10.0 *0.4);

  // Compute intensity, mostly on the bottom
  float w = 2.0 * r * p.y;

  // Smooth out left/right side and fade in at start
  w /= smoothstep(0.0,0.1, xy.x)* smoothstep(0.0,0.1, 1.0-xy.x) *  smoothstep(0.0,0.4, u_time);

  // Compute colors
  vec3 c = vec3(1.0,.2,.05);
  vec3 color = 1.0 / (w*w/c + 1.0);

  // Mix in widget
  fragColor = gsk_premultiply(vec4(color, color.x));
}

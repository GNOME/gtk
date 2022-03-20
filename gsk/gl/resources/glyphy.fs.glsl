// FRAGMENT_SHADER:
// glyphy.fs.glsl

uniform float u_contrast;
uniform float u_gamma_adjust;
uniform float u_outline_thickness;
uniform bool  u_outline;
uniform float u_boldness;
uniform bool  u_debug;

_IN_ vec4 v_glyph;
_IN_ vec4 final_color;

#define SQRT2_2 0.70710678118654757 /* 1 / sqrt(2.) */
#define SQRT2   1.4142135623730951

struct glyph_info_t {
  ivec2 nominal_size;
  ivec2 atlas_pos;
};

glyph_info_t
glyph_info_decode (vec4 v)
{
  glyph_info_t gi;
  gi.nominal_size = (ivec2 (mod (v.zw, 256.)) + 2) / 4;
  gi.atlas_pos = ivec2 (v_glyph.zw) / 256;
  return gi;
}


float
antialias (float d)
{
  return smoothstep (-.75, +.75, d);
}

void
main()
{
  vec2 p = v_glyph.xy;
  glyph_info_t gi = glyph_info_decode (v_glyph);

  /* isotropic antialiasing */
  vec2 dpdx = dFdx (p);
  vec2 dpdy = dFdy (p);
  float m = length (vec2 (length (dpdx), length (dpdy))) * SQRT2_2;

  float gsdist = glyphy_sdf (p, gi.nominal_size GLYPHY_DEMO_EXTRA_ARGS);
  float sdist = gsdist / m * u_contrast;

  if (!u_debug) {
    sdist -= u_boldness * 10.;
    if (u_outline)
      sdist = abs (sdist) - u_outline_thickness * .5;
    if (sdist > 1.)
      discard;
    float alpha = antialias (-sdist);
    if (u_gamma_adjust != 1.)
      alpha = pow (alpha, 1./u_gamma_adjust);

    gskSetOutputColor(final_color * alpha);
  } else {
    vec4 color = vec4 (0,0,0,0);

    // Color the inside of the glyph a light red
    color += vec4 (.5,0,0,.5) * smoothstep (1., -1., sdist);

    float udist = abs (sdist);
    float gudist = abs (gsdist);
    // Color the outline red
    color += vec4 (1,0,0,1) * smoothstep (2., 1., udist);
    // Color the distance field in green
    if (!glyphy_isinf (udist))
      color += vec4(0,.4,0,.4 - (abs(gsdist) / max(float(gi.nominal_size.x), float(gi.nominal_size.y))) * 4.);

    float pdist = glyphy_point_dist (p, gi.nominal_size GLYPHY_DEMO_EXTRA_ARGS);
    // Color points green
    color = mix (vec4 (0,1,0,.5), color, smoothstep (.05, .06, pdist));

    glyphy_arc_list_t arc_list = glyphy_arc_list (p, gi.nominal_size GLYPHY_DEMO_EXTRA_ARGS);
    // Color the number of endpoints per cell blue
    color += vec4 (0,0,1,.1) * float(arc_list.num_endpoints) * 32./255.;

    gskSetOutputColor(color);
  }
}

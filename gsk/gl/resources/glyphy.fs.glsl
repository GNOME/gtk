// FRAGMENT_SHADER:
// glyphy.fs.glsl

uniform float u_contrast;
uniform float u_gamma_adjust;
uniform float u_outline_thickness;
uniform bool  u_outline;
uniform float u_boldness;

_IN_ vec4 v_glyph;
_IN_ vec4 final_color;

#define SQRT2     1.4142135623730951
#define SQRT2_INV 0.70710678118654757 /* 1 / sqrt(2.) */

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
  float m = length (vec2 (length (dpdx), length (dpdy))) * SQRT2_INV;

  float gsdist = glyphy_sdf (p, gi.nominal_size GLYPHY_DEMO_EXTRA_ARGS);
  gsdist -= u_boldness;
  float sdist = gsdist / m * u_contrast;

  if (u_outline)
    sdist = abs (sdist) - u_outline_thickness * .5;

  if (sdist > 1.)
    discard;

  float alpha = antialias (-sdist);
  if (u_gamma_adjust != 1.)
    alpha = pow (alpha, 1./u_gamma_adjust);

  gskSetOutputColor(final_color * alpha);
}

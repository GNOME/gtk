#define GSK_N_TEXTURES 0

#include "common.glsl"

/* blur radius (aka in_blur_direction) 0 is NOT supported and MUST be caught before */

#define VARIATION_INSET ((GSK_VARIATION & 1u) == 1u)

PASS(0) vec2 _pos;
PASS_FLAT(1) RoundedRect _shadow_outline;
PASS_FLAT(4) RoundedRect _clip_outline;
PASS_FLAT(7) vec4 _color;
PASS_FLAT(8) vec2 _sigma;

#ifdef GSK_VERTEX_SHADER

IN(0) mat3x4 in_outline;
IN(3) vec4 in_bounds;
IN(4) vec4 in_color;
IN(5) vec2 in_shadow_offset;
IN(6) float in_shadow_spread;
IN(7) float in_blur_radius;

#define GAUSSIAN_SCALE_FACTOR ((3.0 * sqrt(2.0 * PI) / 4.0))

void
run (out vec2 pos)
{
  Rect bounds = rect_from_gsk (in_bounds);
  RoundedRect outside = rounded_rect_from_rect (bounds);
  RoundedRect outline = rounded_rect_from_gsk (in_outline);
  vec2 spread = GSK_GLOBAL_SCALE * in_shadow_spread;
  
  _clip_outline = outline;

  RoundedRect inside;
  if (!VARIATION_INSET)
    {
      inside = outline;
      spread = -spread;
      outline = rounded_rect_shrink (outline, spread.yxyx);
      rounded_rect_offset (outline, GSK_GLOBAL_SCALE * in_shadow_offset);
    }
  else
    {
      outline = rounded_rect_shrink (outline, spread.yxyx);
      rounded_rect_offset (outline, GSK_GLOBAL_SCALE * in_shadow_offset);
      inside = outline;
      inside = rounded_rect_shrink (inside, (in_blur_radius * GAUSSIAN_SCALE_FACTOR * GSK_GLOBAL_SCALE).yxyx);
    }

  pos = border_get_position (outside, inside);

  _pos = pos;
  _shadow_outline = outline;
  _color = output_color_from_alt (in_color);
  _sigma = GSK_GLOBAL_SCALE * 0.5 * in_blur_radius;
}

#endif


#ifdef GSK_FRAGMENT_SHADER

/* A standard gaussian function, used for weighting samples */
float
gauss (float x,
       float sigma)
{
  float sigma_2 = sigma * sigma;
  return 1.0 / sqrt (2.0 * PI * sigma_2) * exp (-(x * x) / (2.0 * sigma_2));
}

/* This approximates the error function, needed for the gaussian integral */
vec2
erf (vec2 x)
{
  vec2 s = sign(x), a = abs(x);
  x = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
  x *= x;
  return s - s / (x * x);
}

float
erf_range (vec2 x,
           float sigma)
{
  vec2 from_to = 0.5 - 0.5 * erf (x / (sigma * SQRT1_2));
  return from_to.y - from_to.x;
}

float
ellipse_x (vec2  ellipse,
           float y)
{
  float y_scaled = y / ellipse.y;
  return ellipse.x * sqrt (1.0 - y_scaled * y_scaled);
}

float
blur_rect (Rect r,
           vec2 pos)
{
  return erf_range (rect_bounds (r).xz - pos.x, _sigma.x) * erf_range (rect_bounds (r).yw - pos.y, _sigma.y);
}

float
blur_corner (vec2 p,
             vec2 r)
{
  if (min (r.x, r.y) <= 0.0)
    return 0.0;

  p /= _sigma;
  r /= _sigma;

  if (min (p.x, p.y) <= -2.95 ||
      max (p.x - r.x, p.y - r.y) >= 2.95)
    return 0.0;

  float result = 0.0;
  float start = max (p.y - 3.0, 0.0);
  float end = min (p.y + 3.0, r.y);
  float step = (end - start) / 7.0;
  float y = start;
  for (int i = 0; i < 8; i++)
    {
      float x = r.x - ellipse_x (r, r.y - y);
      result -= gauss (p.y - y, 1.0) * erf_range (vec2 (- p.x, x - p.x), 1.0);
      y += step;
    }
  return step * result;
}

float
blur_rounded_rect (RoundedRect r,
                   vec2        p)
{
  float result = blur_rect (Rect (rounded_rect_bounds (r)), _pos);

  result -= blur_corner (p - rounded_rect_bounds (r).xy, rounded_rect_corner (r, TOP_LEFT));
  result -= blur_corner (vec2 (rounded_rect_bounds (r).z - p.x, p.y - rounded_rect_bounds (r).y), rounded_rect_corner (r, TOP_RIGHT));
  result -= blur_corner (rounded_rect_bounds (r).zw - p, rounded_rect_corner (r, BOTTOM_RIGHT));
  result -= blur_corner (vec2 (p.x - rounded_rect_bounds (r).x, rounded_rect_bounds (r).w - p.y), rounded_rect_corner (r, BOTTOM_LEFT));

  return result;
}


void
run (out vec4 color,
     out vec2 position)
{
  float clip_alpha = rounded_rect_coverage (_clip_outline, _pos);

  if (!VARIATION_INSET)
    clip_alpha = 1.0 - clip_alpha;

  if (clip_alpha == 0.0)
    {
      color = vec4 (0.0);
      position = _pos;
      return;
    }

  float blur_alpha = blur_rounded_rect (_shadow_outline, _pos);
  if (VARIATION_INSET)
    blur_alpha = 1.0 - blur_alpha;

  color = output_color_alpha (_color, clip_alpha * blur_alpha);
  position = _pos;
}

#endif

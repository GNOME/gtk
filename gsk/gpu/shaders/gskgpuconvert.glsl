#include "common.glsl"

#define VARIATION_OPACITY              (1u << 0)
#define VARIATION_ALPHA_ALL_CHANNELS   (1u << 1)
#define VARIATION_SOURCE_UNPREMULTIPLY (1u << 2)
#define VARIATION_TARGET_PREMULTIPLY   (1u << 3)
#define VARIATION_SOURCE_SHIFT 8u
#define VARIATION_TARGET_SHIFT 16u
#define VARIATION_COLOR_STATE_MASK 0xFFu

#define HAS_VARIATION(var) ((GSK_VARIATION & var) == var)

#define SOURCE_COLOR_STATE ((GSK_VARIATION >> VARIATION_SOURCE_SHIFT) & VARIATION_COLOR_STATE_MASK)
#define TARGET_COLOR_STATE ((GSK_VARIATION >> VARIATION_TARGET_SHIFT) & VARIATION_COLOR_STATE_MASK)

float
srgb_eotf (float v)
{
  if (v >= 0.04045)
    return pow (((v + 0.055) / (1.0 + 0.055)), 2.4);
  else
    return v / 12.92;
}

float
srgb_oetf (float v)
{
  if (v > 0.0031308)
    return 1.055 * pow (v, 1.0 / 2.4) - 0.055;
  else
    return 12.92 * v;
}

vec4
srgb_to_linear_srgb (vec4 color)
{
  return vec4 (srgb_eotf (color.r),
               srgb_eotf (color.g),
               srgb_eotf (color.b),
               color.a);
}

vec4
linear_srgb_to_srgb (vec4 color)
{
  return vec4 (srgb_oetf (color.r),
               srgb_oetf (color.g),
               srgb_oetf (color.b),
               color.a);
}

#define PAIR(_from_cs, _to_cs) ((_from_cs) << 16 | (_to_cs))

bool
do_conversion (vec4     color,
               uint     from_cs,
               uint     to_cs,
               out vec4 result)
{
  switch (PAIR (from_cs, to_cs))
    {
    case PAIR (GDK_COLOR_STATE_ID_SRGB, GDK_COLOR_STATE_ID_SRGB_LINEAR):
      result = srgb_to_linear_srgb (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_SRGB_LINEAR, GDK_COLOR_STATE_ID_SRGB):
      result = linear_srgb_to_srgb (color);
      break;

    default:
      return false;
    }

  return true;
}

vec4
color_convert (vec4 color)
{
  vec4 result;

  if (SOURCE_COLOR_STATE == TARGET_COLOR_STATE)
    return color;

  if (!do_conversion (color, SOURCE_COLOR_STATE, TARGET_COLOR_STATE, result))
    result = vec4 (1.0, 0.0, 0.8, 1.0);

  return result;
}

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS(2) vec2 _tex_coord;
PASS_FLAT(3) uint _tex_id;
PASS_FLAT(4) float _opacity;

#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_tex_rect;
IN(2) uint in_tex_id;
IN(3) float in_opacity;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);

  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
  _tex_id = in_tex_id;
  _opacity = in_opacity;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 pixel = gsk_texture (_tex_id, _tex_coord);

  if (HAS_VARIATION (VARIATION_SOURCE_UNPREMULTIPLY))
    pixel = color_unpremultiply (pixel);

  pixel = color_convert (pixel);

  float alpha = rect_coverage (_rect, _pos);
  if (HAS_VARIATION (VARIATION_OPACITY))
    alpha *= _opacity;

  if (HAS_VARIATION (VARIATION_ALPHA_ALL_CHANNELS))
    pixel *= alpha;
  else
    pixel.a *= alpha;

  if (HAS_VARIATION (VARIATION_TARGET_PREMULTIPLY))
    color = color_premultiply (pixel);
  else
    color = pixel;

  position = _pos;
}

#endif

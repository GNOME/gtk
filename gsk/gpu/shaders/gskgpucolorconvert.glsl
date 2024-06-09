#include "common.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS(2) vec2 _tex_coord;
PASS_FLAT(3) uint _tex_id;

#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_tex_rect;
IN(2) uint in_tex_id;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);

  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
  _tex_id = in_tex_id;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

float
srgb_transfer_function (float v)
{
  if (v >= 0.04045)
    return pow (((v + 0.055)/(1.0 + 0.055)), 2.4);
  else
    return v / 12.92;
}

float
srgb_inverse_transfer_function (float v)
{
  if (v > 0.0031308)
    return 1.055 * pow (v, 1.0/2.4) - 0.055;
  else
    return 12.92 * v;
}

vec4
srgb_to_linear_srgb (vec4 color)
{
  return vec4 (srgb_transfer_function (color.r),
               srgb_transfer_function (color.g),
               srgb_transfer_function (color.b),
               color.a);
}

vec4
linear_srgb_to_srgb (vec4 color)
{
  return vec4 (srgb_inverse_transfer_function (color.r),
               srgb_inverse_transfer_function (color.g),
               srgb_inverse_transfer_function (color.b),
               color.a);
}

#define RGB         0u
#define LINRGB      1u

#define pair(u,v) ((u)|((v) << 16))
#define first(p) ((p) & 0xffffu)
#define second(p) ((p) >> 16)

bool
do_conversion (in vec4 color, in uint f, out vec4 result)
{
  switch (f)
    {
    case pair (RGB, LINRGB): result = srgb_to_linear_srgb (color); break;
    case pair (LINRGB, RGB): result = linear_srgb_to_srgb (color); break;

    default: return false;
    }

  return true;
}

vec4
color_convert (in vec4 color, in uint f)
{
  vec4 result;

  if (f == 0u)
    return color;

  do_conversion (color, f, result);

  return result;
}

void
run (out vec4 color,
     out vec2 position)
{
  vec4 pixel = gsk_texture (_tex_id, _tex_coord);

  pixel = color_unpremultiply (pixel);
  pixel = color_convert (pixel, GSK_VARIATION);

  color = color_premultiply (pixel) * rect_coverage (_rect, _pos);
  position = _pos;
}

#endif

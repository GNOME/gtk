#define GSK_N_TEXTURES 1

#include "common.glsl"

#define VARIATION_COLOR_SPACE_MASK     (0xFFu)
#define VARIATION_OPACITY              (1u << 8)
#define VARIATION_STRAIGHT_ALPHA       (1u << 9)
#define VARIATION_PREMULTIPLY          (1u << 10)
#define VARIATION_REVERSE              (1u << 11)

#define HAS_VARIATION(var) ((GSK_VARIATION & var) == var)
#define BUILTIN_COLOR_SPACE (GSK_VARIATION & VARIATION_COLOR_SPACE_MASK)

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS(2) vec2 _tex_coord;
PASS_FLAT(3) float _opacity;

#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_tex_rect;
IN(2) float in_opacity;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);

  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
  _opacity = in_opacity;
}

#endif


#ifdef GSK_FRAGMENT_SHADER

/* Note that these matrices are transposed from the C version */

const mat3 srgb_to_lms = mat3(
  0.4122214708, 0.2119034982, 0.0883024619,
  0.5363325363, 0.6806995451, 0.2817188376,
  0.0514459929, 0.1073969566, 0.6299787005
);

const mat3 lms_to_oklab = mat3(
  0.2104542553,  1.9779984951,  0.0259040371,
  0.7936177850, -2.4285922050,  0.7827717662,
 -0.0040720468,  0.4505937099, -0.8086757660
);

const mat3 oklab_to_lms = mat3(
  1.0,           1.0,           1.0,
  0.3963377774, -0.1055613458, -0.0894841775,
  0.2158037573, -0.0638541728, -1.2914855480
);

const mat3 lms_to_srgb = mat3(
  4.0767416621, -1.2684380046, -0.0041960863,
 -3.3077115913,  2.6097574011, -0.7034186147,
  0.2309699292, -0.3413193965,  1.7076147010
);

vec3
oklab_to_srgb_linear (vec3 color)
{
  vec3 lms = oklab_to_lms * color;

  lms = vec3 (pow (lms.r, 3.0),
              pow (lms.g, 3.0),
              pow (lms.b, 3.0));

  return lms_to_srgb * lms;
}

vec3
srgb_linear_to_oklab (vec3 color)
{
  vec3 lms = srgb_to_lms * color;

  lms = vec3 (pow (lms.r, 1.0/3.0),
              pow (lms.g, 1.0/3.0),
              pow (lms.b, 1.0/3.0));

  return lms_to_oklab * lms;
}

#define M_PI 3.14159265358979323846
#define RAD_TO_DEG(x) ((x)*180.0/M_PI)
#define DEG_TO_RAD(x) ((x)*M_PI/180.0)

float
normalize_hue (float h)
{
  while (h < 0.0)
    h += 360.0;
  while (h > 360.0)
    h -= 360.0;
  return h;
}

vec3
oklch_to_oklab (vec3 color)
{
  color.z = normalize_hue (color.z);

  return vec3 (color.x,
               color.y * cos (DEG_TO_RAD (color.z)),
               color.y * sin (DEG_TO_RAD (color.z)));
}

vec3
oklab_to_oklch (vec3 color)
{
  return vec3 (color.x,
               length (color.yz),
               RAD_TO_DEG (atan (color.z, color.y)));
}

vec3
oklch_to_srgb_linear (vec3 color)
{
  return oklab_to_srgb_linear (oklch_to_oklab (color));
}

vec3
srgb_linear_to_oklch (vec3 color)
{
  return oklab_to_oklch (srgb_linear_to_oklab (color));
}

vec4
convert_color_from_builtin (vec4 color)
{
  color = color_unpremultiply (color);

  switch (BUILTIN_COLOR_SPACE)
    {
    case GDK_BUILTIN_COLOR_STATE_ID_OKLAB:
      return vec4 (oklab_to_srgb_linear (color.rgb), color.a);

    case GDK_BUILTIN_COLOR_STATE_ID_OKLCH:
      return vec4 (oklch_to_srgb_linear (color.rgb), color.a);

    default:
      return vec4(0.0, 1.0, 0.8, 1.0);
    }
}

vec4
convert_color_to_builtin (vec4 color)
{
  switch (BUILTIN_COLOR_SPACE)
    {
    case GDK_BUILTIN_COLOR_STATE_ID_OKLAB:
      return vec4 (srgb_linear_to_oklab (color.rgb), color.a);

    case GDK_BUILTIN_COLOR_STATE_ID_OKLCH:
      return vec4 (srgb_linear_to_oklch (color.rgb), color.a);

    default:
      return vec4(0.0, 1.0, 0.8, 1.0);
    }
}

void
run (out vec4 color,
     out vec2 position)
{
  vec4 pixel = gsk_texture0 (_tex_coord);

  if (HAS_VARIATION (VARIATION_REVERSE))
    {
      pixel = alt_color_from_output (pixel);
      pixel = convert_color_to_builtin (pixel);
    }
  else
    {
      pixel = convert_color_from_builtin (pixel);
      pixel = output_color_from_alt (pixel);
    }

  float alpha = rect_coverage (_rect, _pos);
  if (HAS_VARIATION (VARIATION_OPACITY))
    alpha *= _opacity;

  color = output_color_alpha (pixel, alpha);

  if (HAS_VARIATION (VARIATION_PREMULTIPLY))
    color_premultiply (color);

  position = _pos;
}

#endif

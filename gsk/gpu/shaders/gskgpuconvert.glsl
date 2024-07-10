#include "common.glsl"

#define VARIATION_OPACITY              (1u << 0)
#define VARIATION_STRAIGHT_ALPHA       (1u << 1)

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
srgb_to_srgb_linear (vec4 color)
{
  return vec4 (srgb_eotf (color.r),
               srgb_eotf (color.g),
               srgb_eotf (color.b),
               color.a);
}

vec4
srgb_linear_to_srgb (vec4 color)
{
  return vec4 (srgb_oetf (color.r),
               srgb_oetf (color.g),
               srgb_oetf (color.b),
               color.a);
}

vec4
srgb_linear_to_xyz (vec4 color)
{
  mat3 m = mat3 (506752.0 / 1228815.0,  87881.0 / 245763.0,   12673.0 /   70218.0,
                  87098.0 /  409605.0, 175762.0 / 245763.0,   12673.0 /  175545.0,
                   7918.0 /  409605.0,  87881.0 / 737289.0, 1001167.0 / 1053270.0);

  return vec4 (color.rgb * m, color.a);
}

vec4
xyz_to_srgb_linear (vec4 color)
{
  mat3 m = mat3 (  12831.0 /   3959.0,    -329.0 /    214.0, -1974.0 /   3959.0,
                 -851781.0 / 878810.0, 1648619.0 / 878810.0, 36519.0 / 878810.0,
                     705.0 /  12673.0,   -2585.0 /  12673.0,   705.0 /    667.0);

  return vec4 (color.xyz * m, color.a);
}

#define M_PI 3.1415926535897932384626433832795
#define RAD_TO_DEG(x) ((x)*180.0/M_PI)
#define DEG_TO_RAD(x) ((x)*M_PI/180.0)

vec4
oklab_to_oklch (vec4 color)
{
  return vec4 (color.x,
               length (color.yz),
               RAD_TO_DEG (atan (color.z, color.y)),
               color.a);
}

float
normalize_hue (float h)
{
  while (h < 0.0)
    h += 360.0;
  while (h > 360.0)
    h -= 360.0;
  return h;
}

vec4
oklch_to_oklab (vec4 color)
{
  color.z = normalize_hue (color.z);

  return vec4 (color.x,
               color.y * cos (DEG_TO_RAD (color.z)),
               color.y * sin (DEG_TO_RAD(color.z)),
               color.a);
}

vec4
oklab_to_xyz (vec4 color)
{
  mat3 m1 = mat3 (1.0,  0.3963377774,  0.2158037573,
                  1.0, -0.1055613458, -0.0638541728,
                  1.0, -0.0894841775, -1.2914855480);

  vec3 lms = color.rgb * m1;

  lms = vec3 (pow (lms.x, 3.0),
              pow (lms.y, 3.0),
              pow (lms.z, 3.0));

  mat3 m2 = mat3 ( 4.0767416621, -3.3077115913,  0.2309699292,
                  -1.2684380046,  2.6097574011, -0.3413193965,
                  -0.0041960863, -0.7034186147,  1.7076147010);
  mat3 m3 = mat3 (506752.0 / 1228815.0,  87881.0 / 245763.0,   12673.0 /   70218.0,
                  87098.0 /  409605.0, 175762.0 / 245763.0,   12673.0 /  175545.0,
                   7918.0 /  409605.0,  87881.0 / 737289.0, 1001167.0 / 1053270.0);

  mat3 m4 = m2 * m3;

  vec3 rgb = lms * m4;

  return vec4 (rgb, color.a);
}

vec4
xyz_to_oklab (vec4 color)
{
  mat3 m1 = mat3 (  12831.0 /   3959.0,    -329.0 /    214.0, -1974.0 /   3959.0,
                  -851781.0 / 878810.0, 1648619.0 / 878810.0, 36519.0 / 878810.0,
                      705.0 /  12673.0,   -2585.0 /  12673.0,   705.0 /    667.0);

  mat3 m2 = mat3 (0.4122214708, 0.5363325363, 0.0514459929,
                  0.2119034982, 0.6806995451, 0.1073969566,
                  0.0883024619, 0.2817188376, 0.6299787005);

  mat3 m3 = m1 * m2;

  vec3 lms = color.rgb * m3;

  lms = vec3 (pow (lms.x, 1.0/3.0),
              pow (lms.y, 1.0/3.0),
              pow (lms.z, 1.0/3.0));

  mat3 m4 = mat3 (0.2104542553,  0.7936177850, -0.0040720468,
                  1.9779984951, -2.4285922050,  0.4505937099,
                  0.0259040371,  0.7827717662, -0.8086757660);

  vec3 lab = lms * m4;

  return vec4 (lab, color.a);
}

float
rec2020_eotf (float v)
{
  float alpha = 1.09929682680944;
  float beta = 0.018053968510807;

  float sign = v < 0.0 ? -1.0 : 1.0;
  float vabs = abs (v);

  if (vabs < beta * 4.5)
    return v / 4.5;
  else
    return sign * pow ((vabs + alpha - 1.0) / alpha, 1.0 / 0.45);
}

float
rec2020_oetf (float v)
{
  float alpha = 1.09929682680944;
  float beta = 0.018053968510807;
  float sign = v < 0.0 ? -1.0 : 1.0;
  float vabs = abs (v);

  if (vabs > beta)
    return sign * (alpha * pow (vabs, 0.45) - (alpha - 1.0));
  else
    return 4.5 * v;
}

vec4
rec2020_to_rec2020_linear (vec4 color)
{
  return vec4 (rec2020_eotf (color.r),
               rec2020_eotf (color.g),
               rec2020_eotf (color.b),
               color.a);
}

vec4
rec2020_linear_to_rec2020 (vec4 color)
{
  return vec4 (rec2020_oetf (color.r),
               rec2020_oetf (color.g),
               rec2020_oetf (color.b),
               color.a);
}

vec4
rec2020_linear_to_xyz (vec4 color)
{
  mat3 m = mat3 (63426534.0 / 99577255.0,  20160776.0 / 139408157.0,  47086771.0 / 278816314.0,
                 26158966.0 / 99577255.0, 472592308.0 / 697040785.0,   8267143.0 / 139408157.0,
                        0.0,               19567812.0 / 697040785.0, 295819943.0 / 278816314.0);

  return vec4 (color.rgb * m, color.a);
}

vec4
xyz_to_rec2020_linear (vec4 color)
{
  mat3 m = mat3 ( 30757411.0 / 17917100.0, -6372589.0 / 17917100.0,  -4539589.0 / 17917100.0,
                 -19765991.0 / 29648200.0, 47925759.0 / 29648200.0,    467509.0 / 29648200.0,
                    792561.0 / 44930125.0, -1921689.0 / 44930125.0,  42328811.0 / 44930125.0);

  return vec4 (color.xyz * m, color.z);
}

#define CONCAT(f, f1, f2) vec4 f(vec4 color) { return f2(f1(color)); }

CONCAT(srgb_to_xyz, srgb_to_srgb_linear, srgb_linear_to_xyz)
CONCAT(xyz_to_srgb, xyz_to_srgb_linear, srgb_linear_to_srgb)
CONCAT(oklch_to_xyz, oklch_to_oklab, oklab_to_xyz)
CONCAT(xyz_to_oklch, xyz_to_oklab, oklab_to_oklch)
CONCAT(rec2020_to_xyz, rec2020_to_rec2020_linear, rec2020_linear_to_xyz)
CONCAT(xyz_to_rec2020, xyz_to_rec2020_linear, rec2020_linear_to_rec2020)

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
      result = srgb_to_srgb_linear (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_SRGB_LINEAR, GDK_COLOR_STATE_ID_SRGB):
      result = srgb_linear_to_srgb (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_SRGB_LINEAR, GDK_COLOR_STATE_ID_XYZ):
      result = srgb_linear_to_xyz (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_SRGB, GDK_COLOR_STATE_ID_XYZ):
      result = srgb_to_xyz (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_OKLAB, GDK_COLOR_STATE_ID_XYZ):
      result = oklab_to_xyz (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_OKLCH, GDK_COLOR_STATE_ID_XYZ):
      result = oklch_to_xyz (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_REC2020, GDK_COLOR_STATE_ID_XYZ):
      result = rec2020_to_xyz (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_REC2020_LINEAR, GDK_COLOR_STATE_ID_XYZ):
      result = rec2020_linear_to_xyz (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_XYZ, GDK_COLOR_STATE_ID_SRGB_LINEAR):
      result = xyz_to_srgb_linear (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_XYZ, GDK_COLOR_STATE_ID_SRGB):
      result = xyz_to_srgb (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_XYZ, GDK_COLOR_STATE_ID_OKLAB):
      result = xyz_to_oklab (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_XYZ, GDK_COLOR_STATE_ID_OKLCH):
      result = xyz_to_oklch (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_XYZ, GDK_COLOR_STATE_ID_REC2020):
      result = xyz_to_rec2020 (color);
      break;
    case PAIR (GDK_COLOR_STATE_ID_XYZ, GDK_COLOR_STATE_ID_REC2020_LINEAR):
      result = xyz_to_rec2020_linear (color);
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
    {
      do_conversion (color, SOURCE_COLOR_STATE, GDK_COLOR_STATE_ID_XYZ, result);
      do_conversion (result, GDK_COLOR_STATE_ID_XYZ, TARGET_COLOR_STATE, result);
    }

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
  vec4 pixel;
  if (HAS_VARIATION (VARIATION_STRAIGHT_ALPHA))
    pixel = gsk_texture_straight_alpha (_tex_id, _tex_coord);
  else
    pixel = gsk_texture (_tex_id, _tex_coord);

  pixel = output_color_from_alt (pixel);

  float alpha = rect_coverage (_rect, _pos);
  if (HAS_VARIATION (VARIATION_OPACITY))
    alpha *= _opacity;

  color = output_color_alpha (pixel, alpha);

  position = _pos;
}

#endif

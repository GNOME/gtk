#ifndef _COLOR_
#define _COLOR_

vec4
color_premultiply (vec4 color)
{
  return vec4 (color.rgb, 1.0) * color.a;
}

vec4
color_unpremultiply (vec4 color)
{
  return color.a > 0.0 ? color / vec4 (color.aaa, 1.0) : color;
}

float
luminance (vec3 color)
{
  return dot (vec3 (0.2126, 0.7152, 0.0722), color);
}

vec4
rgb_to_hsl (vec4 color)
{
  float m0;
  float m1;
  float delta;
  vec4 result;

  m1 = max (max (color.r, color.g), color.b);
  m0 = min (min (color.r, color.g), color.b);

  result.x = 0.0;
  result.y = 0.0;
  result.z = (m1 + m0) / 2.0;
  result.a = color.a;

  if (m1 != m0)
    {
      if (result.z <= 0.5)
        result.y = (m1 - m0) / (m1 + m0);
      else
        result.y = (m1 - m0) / (2.0 - m1 - m0);

      delta = m1 - m0;
      if (color.r == m1)
        result.x = (color.g - color.b) / delta;
      else if (color.g == m1)
        result.x = 2.0 + (color.b - color.r) / delta;
      else if (color.g == m1)
        result.x = 4.0 + (color.r - color.g) / delta;

      result.x *= 60.0;
      if (result.x < 0.0)
        result.x += 360.0;
    }

  return result;
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
hsl_to_rgb (vec4 color)
{
  vec4 result;
  float m1, m2, h;

  color.x = normalize_hue (color.x);

  if (color.z <= 0.5)
    m2 = color.z * (1.0 + color.y);
  else
    m2 = color.z + color.y - color.z * color.y;
  m1 = 2.0 * color.z - m2;

  result.a = color.a;

  if (color.y == 0.0)
    {
      result.r = color.z;
      result.g = color.z;
      result.b = color.z;
    }
  else
    {
      h = color.x + 120.0;
      while (h > 360.0)
        h -= 360.0;
      while (h < 0.0)
        h += 360.0;

      if (h < 60.0)
        result.r = m1 + (m2 - m1) * h / 60.0;
      else if (h < 180.0)
        result.r = m2;
      else if (h < 240.0)
        result.r = m1 + (m2 - m1) * (240.0 - h) / 60.0;
      else
        result.r = m1;

      h = color.x;
      while (h > 360.0)
        h -= 360.0;
      while (h < 0.0)
        h += 360.0;

      if (h < 60.)
        result.g = m1 + (m2 - m1) * h / 60.0;
      else if (h < 180.0)
        result.g = m2;
      else if (h < 240.0)
        result.g = m1 + (m2 - m1) * (240.0 - h) / 60.0;
      else
        result.g = m1;

      h = color.x - 120.0;
      while (h > 360.0)
        h -= 360.0;
      while (h < 0.0)
        h += 360.0;

      if (h < 60.)
        result.b = m1 + (m2 - m1) * h / 60.0;
      else if (h < 180.0)
        result.b = m2;
      else if (h < 240.0)
        result.b = m1 + (m2 - m1) * (240.0 - h) / 60.0;
      else
        result.b = m1;
    }

  return result;
}

vec4
rgb_to_hwb (vec4 color)
{
  vec4 hsl = rgb_to_hsl (color);
  vec4 result;

  result.x = hsl.x;
  result.y = min (min (color.x, color.y), color.z);
  result.z = max (max (color.x, color.y), color.z);

  return result;
}

vec4
hwb_to_rgb (vec4 color)
{
  vec4 result;
  float f;

  color.x = normalize_hue (color.x);

  if (color.y + color.z >= 1.0)
    {
      float gray = color.y / (color.y + color.z);

      return vec4 (gray, gray, gray, color.a);
    }

  result = hsl_to_rgb (vec4 (color.x, 1.0, 0.5, color.a));

  f = 1.0 - color.y - color.z;
  result.r = result.r * f + color.y;
  result.g = result.g * f + color.y;
  result.b = result.b * f + color.y;

  return result;
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
oklab_to_linrgb (vec4 color)
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

  vec3 rgb = lms * m2;

  return vec4 (rgb, color.a);
}

vec4
linrgb_to_oklab (vec4 color)
{
  mat3 m1 = mat3 (0.4122214708, 0.5363325363, 0.0514459929,
                  0.2119034982, 0.6806995451, 0.1073969566,
                  0.0883024619, 0.2817188376, 0.6299787005);

  vec3 lms = color.rgb * m1;

  lms = vec3 (pow (lms.x, 1.0/3.0),
              pow (lms.y, 1.0/3.0),
              pow (lms.z, 1.0/3.0));

  mat3 m2 = mat3 (0.2104542553,  0.7936177850, -0.0040720468,
                  1.9779984951, -2.4285922050,  0.4505937099,
                  0.0259040371,  0.7827717662, -0.8086757660);

  vec3 lab = lms * m2;

  return vec4 (lab, color.a);
}

float
apply_gamma (float v)
{
  if (v > 0.0031308)
    return 1.055 * pow (v, 1.0/2.4) - 0.055;
  else
    return 12.92 * v;
}

float
unapply_gamma (float v)
{
  if (v >= 0.04045)
    return pow (((v + 0.055)/(1.0 + 0.055)), 2.4);
  else
    return v / 12.92;
}

vec4
rgb_to_linrgb (vec4 color)
{
  return vec4 (unapply_gamma (color.r),
               unapply_gamma (color.g),
               unapply_gamma (color.b),
               color.a);
}

vec4
linrgb_to_rgb (vec4 color)
{
  return vec4 (apply_gamma (color.r),
               apply_gamma (color.g),
               apply_gamma (color.b),
               color.a);
}

vec4
linrgb_to_xyz (vec4 color)
{
  mat3 m = mat3 (506752.0 / 1228815.0,  87881.0 / 245763.0,   12673.0 /   70218.0,
                  87098.0 /  409605.0, 175762.0 / 245763.0,   12673.0 /  175545.0,
                   7918.0 /  409605.0,  87881.0 / 737289.0, 1001167.0 / 1053270.0);

  return vec4 (color.rgb * m, color.a);
}

vec4
xyz_to_linrgb (vec4 color)
{
  mat3 m = mat3 (  12831.0 /   3959.0,    -329.0 /    214.0, -1974.0 /   3959.0,
                 -851781.0 / 878810.0, 1648619.0 / 878810.0, 36519.0 / 878810.0,
                     705.0 /  12673.0,   -2585.0 /  12673.0,   705.0 /    667.0);

  return vec4 (color.xyz * m, color.a);
}

vec4
linp3_to_xyz (vec4 color)
{
  mat3 m = mat3 (608311.0 / 1250200.0, 189793.0 / 714400.0,  198249.0 / 1000160.0,
                  35783.0 /  156275.0, 247089.0 / 357200.0,  198249.0 / 2500400.0,
                      0.0,              32229.0 / 714400.0, 5220557.0 / 5000800.0);

  return vec4 (color.rgb * m, color.a);
}

vec4
xyz_to_linp3 (vec4 color)
{
  mat3 m = mat3 ( 446124.0 / 178915.0, -333277.0 / 357830.0, -72051.0 / 178915.0,
                  -14852.0 /  17905.0,   63121.0 /  35810.0,    423.0 /  17905.0,
                   11844.0 / 330415.0,  -50337.0 / 660830.0, 316169.0 / 330415.0);

  return vec4 (color.xyz * m, color.a);
}

float
linearize (float val)
{
  float alpha = 1.09929682680944;
  float beta = 0.018053968510807;

  float sign = val < 0.0 ? -1.0 : 1.0;
  float valpos = abs (val);

  if (valpos < beta * 4.5)
    return val / 4.5;
  else
    return sign * pow ((valpos + alpha - 1.0) / alpha, 1.0 / 0.45);
}

vec4
rec2020_to_linrec2020 (vec4 color)
{
  return vec4 (linearize (color.r),
               linearize (color.g),
               linearize (color.b),
               color.a);
}

vec4
linrec2020_to_xyz (vec4 color)
{
  mat3 m = mat3 (63426534.0 / 99577255.0,  20160776.0 / 139408157.0,  47086771.0 / 278816314.0,
                 26158966.0 / 99577255.0, 472592308.0 / 697040785.0,   8267143.0 / 139408157.0,
                        0.0,               19567812.0 / 697040785.0, 295819943.0 / 278816314.0);

  return vec4 (color.rgb * m, color.a);
}

float
delinearize (float val)
{
  float alpha = 1.09929682680944;
  float beta = 0.018053968510807;
  float sign = val < 0.0 ? -1.0 : 1.0;
  float valpos = abs (val);

  if (valpos > beta)
    return sign * (alpha * pow (valpos, 0.45) - (alpha - 1.0));
  else
    return 4.5 * val;
}

vec4
linrec2020_to_rec2020 (vec4 color)
{
  return vec4 (delinearize (color.r),
               delinearize (color.g),
               delinearize (color.b),
               color.a);
}

vec4
xyz_to_linrec2020 (vec4 color)
{
  mat3 m = mat3 ( 30757411.0 / 17917100.0, -6372589.0 / 17917100.0,  -4539589.0 / 17917100.0,
                 -19765991.0 / 29648200.0, 47925759.0 / 29648200.0,    467509.0 / 29648200.0,
                    792561.0 / 44930125.0, -1921689.0 / 44930125.0,  42328811.0 / 44930125.0);

  return vec4 (color.xyz * m, color.z);
}

float
unapply_pq (float v)
{
  float ninv = 16384.0 / 2610.0;
  float minv = 32.0 / 2523.0;
  float c1 = 3424.0 / 4096.0;
  float c2 = 2413.0 / 128.0;
  float c3 = 2392.0 / 128.0;

  float x = pow (max ((pow (v, minv) - c1), 0.0) / (c2 - (c3 * (pow (v, minv)))), ninv);
  return x * 10000.0 / 203.0;
}

vec4
rec2100_to_linrec2100 (vec4 color)
{
  return vec4 (unapply_pq (color.r),
               unapply_pq (color.g),
               unapply_pq (color.b),
               color.a);
}

float
apply_pq (float v)
{
  float x = v * 203.0 / 10000.0;
  float n = 2610.0 / 16384.0;
  float m = 2523.0 / 32.0;
  float c1 = 3424.0 / 4096.0;
  float c2 = 2413.0 / 128.0;
  float c3 = 2392.0 / 128.0;

  return pow (((c1 + (c2 * pow (x, n))) / (1.0 + (c3 * pow (x, n)))), m);
}

vec4
linrec2100_to_rec2100 (vec4 color)
{
  return vec4 (apply_pq (color.r),
               apply_pq (color.g),
               apply_pq (color.b),
               color.a);
}

vec4
linrec2020_to_linrec2100 (vec4 color)
{
  return color;
}

vec4
linrec2100_to_linrec2020 (vec4 color)
{
  return color;
}

#define two_step(name,step1,step2)      \
vec4 name(vec4 color) {                 \
  return step2(step1(color));           \
}

two_step (rgb_to_xyz, rgb_to_linrgb, linrgb_to_xyz)
two_step (hsl_to_xyz, hsl_to_rgb, rgb_to_xyz)
two_step (hwb_to_xyz, hwb_to_rgb, rgb_to_xyz)
two_step (oklab_to_xyz, oklab_to_linrgb, linrgb_to_xyz)
two_step (oklch_to_xyz, oklch_to_oklab, oklab_to_xyz)
two_step (rec2020_to_xyz, rec2020_to_linrec2020, linrec2020_to_xyz)
two_step (linrec2100_to_xyz, linrec2100_to_linrec2020, linrec2020_to_xyz)
two_step (rec2100_to_xyz, rec2100_to_linrec2100, linrec2100_to_xyz)
two_step (p3_to_xyz, rgb_to_linrgb, linp3_to_xyz)
two_step (xyz_to_p3, xyz_to_linp3, linrgb_to_rgb)

two_step (xyz_to_rgb, xyz_to_linrgb, linrgb_to_rgb)
two_step (xyz_to_hsl, xyz_to_rgb, rgb_to_hsl)
two_step (xyz_to_hwb, xyz_to_rgb, rgb_to_hwb)
two_step (xyz_to_oklab, xyz_to_linrgb, linrgb_to_oklab)
two_step (xyz_to_oklch, xyz_to_oklab, oklab_to_oklch)
two_step (xyz_to_rec2020, xyz_to_linrec2020, linrec2020_to_rec2020)
two_step (xyz_to_linrec2100, xyz_to_linrec2020, linrec2020_to_linrec2100)
two_step (xyz_to_rec2100, xyz_to_linrec2100, linrec2100_to_rec2100)

#define RGB         1u
#define LINRGB      3u
#define HSL         5u
#define HWB         7u
#define OKLAB       9u
#define OKLCH      11u
#define P3         13u
#define XYZ        15u
#define REC2020    17u
#define REC2100    19u
#define LINREC2100 21u

#define pair(u,v) ((u)|((v) << 16))
#define first(p) ((p) & 0xffffu)
#define second(p) ((p) >> 16)

bool
do_conversion (in vec4 color, in uint f, out vec4 result)
{
  switch (f)
    {
    case pair (RGB, LINRGB): result = rgb_to_linrgb (color); break;
    case pair (RGB, HSL): result = rgb_to_hsl (color); break;
    case pair (RGB, HWB): result = rgb_to_hwb (color); break;
    case pair (LINRGB, RGB): result = linrgb_to_rgb (color); break;
    case pair (LINRGB, OKLAB): result = linrgb_to_oklab (color); break;
    case pair (HSL, RGB): result = hsl_to_rgb (color); break;
    case pair (HWB, RGB): result = hwb_to_rgb (color); break;
    case pair (OKLAB, LINRGB): result = oklab_to_linrgb (color); break;
    case pair (OKLAB, OKLCH): result = oklab_to_oklch (color); break;
    case pair (OKLCH, OKLAB): result = oklch_to_oklab (color); break;
    case pair (REC2100, LINREC2100): result = rec2100_to_linrec2100 (color); break;
    case pair (LINREC2100, REC2100): result = linrec2100_to_rec2100 (color); break;
    case pair (RGB, XYZ): result = rgb_to_xyz (color); break;
    case pair (LINRGB, XYZ): result = linrgb_to_xyz (color); break;
    case pair (HSL, XYZ): result = hsl_to_xyz (color); break;
    case pair (HWB, XYZ): result = hwb_to_xyz (color); break;
    case pair (OKLAB, XYZ): result = oklab_to_xyz (color); break;
    case pair (OKLCH, XYZ): result = oklch_to_xyz (color); break;
    case pair (P3, XYZ): result = p3_to_xyz (color); break;
    case pair (REC2020, XYZ): result = rec2020_to_xyz (color); break;
    case pair (REC2100, XYZ): result = rec2100_to_xyz (color); break;
    case pair (LINREC2100, XYZ): result = linrec2100_to_xyz (color); break;
    case pair (XYZ, RGB): result = xyz_to_rgb (color); break;
    case pair (XYZ, LINRGB): result = xyz_to_linrgb (color); break;
    case pair (XYZ, HSL): result = xyz_to_hsl (color); break;
    case pair (XYZ, HWB): result = xyz_to_hwb (color); break;
    case pair (XYZ, OKLAB): result = xyz_to_oklab (color); break;
    case pair (XYZ, OKLCH): result = xyz_to_oklch (color); break;
    case pair (XYZ, P3): result = xyz_to_p3 (color); break;
    case pair (XYZ, REC2020): result = xyz_to_rec2020 (color); break;
    case pair (XYZ, REC2100): result = xyz_to_rec2100 (color); break;
    case pair (XYZ, LINREC2100): result = xyz_to_linrec2100 (color); break;

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

  if (!do_conversion (color, f, result))
    {
      vec4 tmp;

      do_conversion (color, pair (first (f), XYZ), tmp);
      do_conversion (tmp, pair(XYZ, second (f)), result);
    }

  return result;
}

#endif /* _COLOR_ */

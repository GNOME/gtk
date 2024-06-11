/* Color utilities
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Simon Budig <Simon.Budig@unix-ag.org> (original code)
 *          Federico Mena-Quintero <federico@gimp.org> (cleanup for GTK+)
 *          Jonathan Blandford <jrb@redhat.com> (cleanup for GTK+)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include "gtkcolorutilsprivate.h"
#include "gdkhslaprivate.h"
#include <math.h>

/* Converts from RGB to HSV */
static void
rgb_to_hsv (float *r,
            float *g,
            float *b)
{
  float red, green, blue;
  float h, s, v;
  float min, max;
  float delta;

  red = *r;
  green = *g;
  blue = *b;

  h = 0.0;

  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;

      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;

      if (red < blue)
        min = red;
      else
        min = blue;
    }

  v = max;

  if (max != 0.0)
    s = (max - min) / max;
  else
    s = 0.0;

  if (s == 0.0)
    h = 0.0;
  else
    {
      delta = max - min;

      if (red == max)
        h = (green - blue) / delta;
      else if (green == max)
        h = 2 + (blue - red) / delta;
      else if (blue == max)
        h = 4 + (red - green) / delta;

      h /= 6.0;

      if (h < 0.0)
        h += 1.0;
      else if (h > 1.0)
        h -= 1.0;
    }

  *r = h;
  *g = s;
  *b = v;
}

/**
 * gtk_hsv_to_rgb:
 * @h: Hue
 * @s: Saturation
 * @v: Value
 * @r: (out): Return value for the red component
 * @g: (out): Return value for the green component
 * @b: (out): Return value for the blue component
 *
 * Converts a color from HSV space to RGB.
 *
 * Input values must be in the [0.0, 1.0] range;
 * output values will be in the same range.
 */
void
gtk_hsv_to_rgb (float  h, float  s, float  v,
                float *r, float *g, float *b)
{
  float hue;
  float f, p;
  int ihue;

  g_return_if_fail (h >= 0.0 && h <= 1.0);
  g_return_if_fail (s >= 0.0 && s <= 1.0);
  g_return_if_fail (v >= 0.0 && v <= 1.0);
  g_return_if_fail (r);
  g_return_if_fail (g);
  g_return_if_fail (b);

  if (s == 0.0)
    {
      *r = v;
      *g = v;
      *b = v;
      return;
    }

  hue = h * 6.0;

  if (hue == 6.0)
    hue = 0.0;

  ihue = (int)hue;
  f = hue - ihue;
  p = v * (1.0 - s);

  if (ihue == 0)
    {
      *r = v;
      *g = v * (1.0 - s * (1.0 - f));
      *b = p;
    }
  else if (ihue == 1)
    {
      *r = v * (1.0 - s * f);
      *g = v;
      *b = p;
    }
  else if (ihue == 2)
    {
      *r = p;
      *g = v;
      *b = v * (1.0 - s * (1.0 - f));
    }
  else if (ihue == 3)
    {
      *r = p;
      *g = v * (1.0 - s * f);
      *b = v;
    }
  else if (ihue == 4)
    {
      *r = v * (1.0 - s * (1.0 - f));
      *g = p;
      *b = v;
    }
  else if (ihue == 5)
    {
      *r = v;
      *g = p;
      *b = v * (1.0 - s * f);
    }
}

/**
 * gtk_rgb_to_hsv:
 * @r: Red
 * @g: Green
 * @b: Blue
 * @h: (out): Return value for the hue component
 * @s: (out): Return value for the saturation component
 * @v: (out): Return value for the value component
 *
 * Converts a color from RGB space to HSV.
 *
 * Input values must be in the [0.0, 1.0] range;
 * output values will be in the same range.
 */
void
gtk_rgb_to_hsv (float  r, float  g, float  b,
                float *h, float *s, float *v)
{
  g_return_if_fail (r >= 0.0 && r <= 1.0);
  g_return_if_fail (g >= 0.0 && g <= 1.0);
  g_return_if_fail (b >= 0.0 && b <= 1.0);

  rgb_to_hsv (&r, &g, &b);

  if (h)
    *h = r;

  if (s)
    *s = g;

  if (v)
    *v = b;
}

void
gtk_rgb_to_hsl (float  red, float  green,      float  blue,
                float *hue, float *saturation, float *lightness)
{
  GdkHSLA hsla;

  _gdk_hsla_init_from_rgba (&hsla, &(GdkRGBA) { red, green, blue, 1.0 });

  *hue = hsla.hue;
  *saturation = hsla.saturation;
  *lightness = hsla.lightness;
}

void
gtk_hsl_to_rgb (float  hue, float  saturation, float  lightness,
                float *red, float *green,      float *blue)
{
  GdkRGBA rgba;

  _gdk_rgba_init_from_hsla (&rgba, &(GdkHSLA) { hue, saturation, lightness, 1.0 });

  *red = rgba.red;
  *green = rgba.green;
  *blue = rgba.blue;
}

void
gtk_rgb_to_hwb (float  red, float  green, float blue,
                float *hue, float *white, float *black)
{
  GdkRGBA rgba = (GdkRGBA) { red, green, blue, 1 };
  GdkHSLA hsla;

  _gdk_hsla_init_from_rgba (&hsla, &rgba);

  *hue = hsla.hue;
  *white = MIN (MIN (red, green), blue);
  *black = (1 - MAX (MAX (red, green), blue));
}

void
gtk_hwb_to_rgb (float  hue, float  white, float  black,
                float *red, float *green, float *blue)
{
  GdkHSLA hsla;
  GdkRGBA rgba;

  if (white + black >= 1)
    {
      float gray = white / (white + black);

      *red = gray;
      *green = gray;
      *blue = gray;

      return;
    }

  hsla.hue = hue;
  hsla.saturation = 1.0;
  hsla.lightness = 0.5;

  _gdk_rgba_init_from_hsla (&rgba, &hsla);

  *red = rgba.red * (1 - white - black) + white;
  *green = rgba.green * (1 - white - black) + white;
  *blue = rgba.blue * (1 - white - black) + white;
}

#define DEG_TO_RAD(x) ((x) * G_PI / 180)
#define RAD_TO_DEG(x) ((x) * 180 / G_PI)

static inline void
_sincosf (float  angle,
          float *out_s,
          float *out_c)
{
#ifdef HAVE_SINCOSF
  sincosf (angle, out_s, out_c);
#else
  *out_s = sinf (angle);
  *out_c = cosf (angle);
#endif
}

void
gtk_oklab_to_oklch (float  L,  float  a, float  b,
                    float *L2, float *C, float *H)
{
  *L2 = L;
  *C = hypotf (a, b);
  *H = RAD_TO_DEG (atan2 (b, a));
  *H = fmod (*H, 360);
  if (*H < 0)
    *H += 360;
}

void
gtk_oklch_to_oklab (float  L,  float  C, float  H,
                    float *L2, float *a, float *b)
{
  *L2 = L;
  _sincosf (DEG_TO_RAD (H), b, a);
  *a *= C;
  *b *= C;
}

static float
apply_gamma (float v)
{
  if (v > 0.0031308)
    return 1.055 * pow (v, 1/2.4) - 0.055;
  else
    return 12.92 * v;
}

static float
unapply_gamma (float v)
{
  if (v >= 0.04045)
    return pow (((v + 0.055)/(1 + 0.055)), 2.4);
  else
    return v / 12.92;
}

void
gtk_oklab_to_linear_srgb (float  L,   float  a,     float  b,
                          float *red, float *green, float *blue)
{
  float l = L + 0.3963377774f * a + 0.2158037573f * b;
  float m = L - 0.1055613458f * a - 0.0638541728f * b;
  float s = L - 0.0894841775f * a - 1.2914855480f * b;

  l = powf (l, 3);
  m = powf (m, 3);
  s = powf (s, 3);

  *red = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
  *green = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
  *blue = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}

void
gtk_oklab_to_rgb (float  L,   float  a,     float  b,
                  float *red, float *green, float *blue)
{
  float linear_red, linear_green, linear_blue;
  gtk_oklab_to_linear_srgb (L, a, b, &linear_red, &linear_green, &linear_blue);
  gtk_linear_srgb_to_rgb (linear_red, linear_green, linear_blue, red, green, blue);
}

void
gtk_linear_srgb_to_oklab (float  red, float  green, float  blue,
                          float *L,   float *a,     float *b)
{
  float l = 0.4122214708f * red + 0.5363325363f * green + 0.0514459929f * blue;
  float m = 0.2119034982f * red + 0.6806995451f * green + 0.1073969566f * blue;
  float s = 0.0883024619f * red + 0.2817188376f * green + 0.6299787005f * blue;

  l = cbrtf (l);
  m = cbrtf (m);
  s = cbrtf (s);

  *L = 0.2104542553f*l + 0.7936177850f*m - 0.0040720468f*s;
  *a = 1.9779984951f*l - 2.4285922050f*m + 0.4505937099f*s;
  *b = 0.0259040371f*l + 0.7827717662f*m - 0.8086757660f*s;
}

void
gtk_rgb_to_oklab (float  red, float  green, float  blue,
                  float *L,   float *a,     float *b)
{
  float linear_red, linear_green, linear_blue;
  gtk_rgb_to_linear_srgb (red, green, blue, &linear_red, &linear_green, &linear_blue);
  gtk_linear_srgb_to_oklab (linear_red, linear_green, linear_blue, L, a, b);
}

void
gtk_rgb_to_linear_srgb (float  red,        float  green,        float  blue,
                        float *linear_red, float *linear_green, float *linear_blue)
{
  *linear_red = unapply_gamma (red);
  *linear_green = unapply_gamma (green);
  *linear_blue = unapply_gamma (blue);
}

void
gtk_linear_srgb_to_rgb (float  linear_red, float  linear_green, float  linear_blue,
                        float *red,        float *green,        float *blue)
{
  *red = apply_gamma (linear_red);
  *green = apply_gamma (linear_green);
  *blue = apply_gamma (linear_blue);
}

void
gtk_linear_srgb_to_xyz (float r, float g, float b,
                        float *x, float *y, float *z)
{
  *x =  (506752.0 / 1228815.0) * r + (87881.0 / 245763.0)  * g + (12673.0 /   70218.0)   * b;
  *y =  (87098.0 /  409605.0)  * r + (175762.0 / 245763.0) * g + (12673.0 /  175545.0)   * b;
  *z =  ( 7918.0 /  409605.0)  * r + (87881.0 / 737289.0)  * g + (1001167.0 / 1053270.0) * b;
}

void
gtk_xyz_to_linear_srgb (float x, float y, float z,
                        float *r, float *g, float *b)
{
  *r =   (12831.0 /   3959.0)  * x - (329.0 /    214.0)     * y - (1974.0 /   3959.0)  * z;
  *g = - (851781.0 / 878810.0) * x + (1648619.0 / 878810.0) * y + (36519.0 / 878810.0) * z;
  *b =   (705.0 /  12673.0)    * x - (2585.0 /  12673.0)    * y + (705.0 /    667.0)   * z;
}

static void
gtk_lin_p3_to_xyz (float r, float g, float b,
                   float *x, float *y, float *z)
{
  *x = (608311.0 / 1250200.0) * r + (189793.0 / 714400.0) * g + (198249.0 / 1000160.0) * b;
  *y = (35783.0 /  156275.0)  * r + (247089.0 / 357200.0) * g + (198249.0 / 2500400.0) * b;
  *z = (      0 /       1)    * r + (32229.0 / 714400.0)  * g + (5220557.0 / 5000800.0) * b;
}

static void
gtk_xyz_to_lin_p3 (float x, float y, float z,
                   float *r, float *g, float *b)
{
  *r =   (446124.0 / 178915.0) * x - (333277.0 / 357830.0) * y - (72051.0 / 178915.0)  * z;
  *g = - (14852.0 /  17905.0)  * x + (63121.0 /  35810.0)  * y + (423.0 /  17905.0)    * z;
  *b =   (11844.0 / 330415.0)  * x - (50337.0 / 660830.0)  * y + (316169.0 / 330415.0) * z;
}

void gtk_rgb_to_p3 (float  red, float  green, float  blue,
                    float *pr,  float *pg,    float *pb)
{
  float r, g, b;
  float x, y, z;

  gtk_rgb_to_linear_srgb (red, green, blue, &r, &g, &b);
  gtk_linear_srgb_to_xyz (r, g, b, &x, &y, &z);
  gtk_xyz_to_lin_p3 (x, y, z, &r, &g, &b);
  gtk_linear_srgb_to_rgb (r, g, b, pr, pg, pb);
}

void
gtk_p3_to_rgb (float  pr,  float  pg,    float pb,
               float *red, float *green, float *blue)
{
  float r, g, b;
  float x, y, z;

  gtk_rgb_to_linear_srgb (pr, pg, pb, &r, &g, &b);
  gtk_lin_p3_to_xyz (r, g, b, &x, &y, &z);
  gtk_xyz_to_linear_srgb (x, y, z, &r, &g, &b);
  gtk_linear_srgb_to_rgb (r, g, b, red, green, blue);
}

static inline float
linearize_one (float val)
{
  float alpha = 1.09929682680944 ;
  float beta = 0.018053968510807;

  int sign = val < 0 ? -1 : 1;
  float abs = fabs (val);

  if (abs < beta * 4.5 )
    return val / 4.5;
  else
    return sign * powf ((abs + alpha - 1) / alpha, 1.0 / 0.45);
}

void
gtk_rec2020_to_rec2020_linear (float r, float g , float b,
                               float *rr, float *gg, float *bb)
{
  *rr = linearize_one (r);
  *gg = linearize_one (g);
  *bb = linearize_one (b);
}

void
gtk_rec2020_linear_to_xyz (float r, float g, float b,
                            float *x, float *y, float *z)
{
  *x = (63426534.0 / 99577255.0) * r + (20160776.0 / 139408157.0)  * g + (47086771.0 / 278816314.0) * b;
  *y = (26158966.0 / 99577255.0) * r + (472592308.0 / 697040785.0) * g + (8267143.0 / 139408157.0) * b;
  *z = (     0 /        1)       * r + (19567812.0 / 697040785.0)  * g + (295819943.0 / 278816314.0) * b;
}

void
gtk_rec2020_to_xyz (float r, float g, float b,
                    float *x, float *y, float *z)
{
  gtk_rec2020_to_rec2020_linear (r, g, b, &r, &g, &b);
  gtk_rec2020_linear_to_xyz (r, g, b, x, y, z);
}

static inline float
delinearize_one (float val)
{
  float alpha = 1.09929682680944;
  float beta = 0.018053968510807;
  int sign = val < 0 ? -1 : 1;
  float abs = fabsf (val);

  if (abs > beta)
    return sign * (alpha * powf (abs, 0.45) - (alpha - 1));
  else
    return 4.5 * val;
}

void
gtk_rec2020_linear_to_rec2020 (float r, float g, float b,
                               float *rr, float *gg, float *bb)
{
  *rr = delinearize_one (r);
  *gg = delinearize_one (g);
  *bb = delinearize_one (b);
}

void
gtk_xyz_to_rec2020_linear (float x, float y, float z,
                           float *r, float *g, float *b)
{
  *r =   (30757411.0 / 17917100.0) * x - (6372589.0 / 17917100.0) * y - (4539589.0 / 17917100.0) * z;
  *g = - (19765991.0 / 29648200.0) * x + (47925759.0 / 29648200.0) * y + (467509.0 / 29648200.0) *z;
  *b =   (792561.0 / 44930125.0)   * x - (1921689.0 / 44930125.0) * y + (42328811.0 / 44930125.0) * z;
}

void
gtk_xyz_to_rec2020 (float x, float y, float z,
                    float *r, float *g, float *b)
{
  gtk_xyz_to_rec2020_linear (x, y, z, r, g, b);
  gtk_rec2020_linear_to_rec2020 (*r, *g, *b, r, g, b);
}

/* maps [0, 1]  to [0, 70] */
static inline float
pq_to_linear (float v)
{
  float ninv = (1 << 14) / 2610.0;
  float minv = (1 << 5) / 2523.0;
  float c1 = 3424.0 / (1 << 12);
  float c2 = 2413.0 / (1 << 7);
  float c3 = 2392.0 / (1 << 7);

  float x = powf (MAX ((powf (v, minv) - c1), 0) / (c2 - (c3 * (powf (v, minv)))), ninv);
  return x * 10000 / 203.0;
}

/* maps [0, 70] to [0, 1] */
void
gtk_rec2100_pq_to_rec2100_linear (float r, float g, float b,
                                  float *rr, float *gg, float *bb)
{
  *rr = pq_to_linear (r);
  *gg = pq_to_linear (g);
  *bb = pq_to_linear (b);
}

static inline float
linear_to_pq (float v)
{
  float x = v * 203.0 / 10000.0;
  float n = 2610.0 / (1 << 14);
  float m = 2523.0 / (1 << 5);
  float c1 = 3424.0 / (1 << 12);
  float c2 = 2413.0 / (1 << 7);
  float c3 = 2392.0 / (1 << 7);

  return powf (((c1 + (c2 * powf (x, n))) / (1 + (c3 * powf (x, n)))), m);
}

void
gtk_rec2100_linear_to_rec2100_pq (float r, float g, float b,
                                  float *rr, float *gg, float *bb)
{
  *rr = linear_to_pq (r);
  *gg = linear_to_pq (g);
  *bb = linear_to_pq (b);
}

void
gtk_rec2100_linear_to_rec2020_linear (float r, float g, float b,
                                      float *rr, float *gg, float *bb)
{
  *rr = r;
  *gg = g;
  *bb = b;
}

void
gtk_rec2020_linear_to_rec2100_linear (float r, float g, float b,
                                      float *rr, float *gg, float *bb)
{
  *rr = r;
  *gg = g;
  *bb = b;
}

/* gdkcolordefs.h
 *
 * Copyright 2024 Matthias Clasen
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

/* Note that this header is shared between the color state implementation
 * and tests, and must not include other headers.
 */

static inline int
sign (float v)
{
  return v < 0 ? -1 : 1;
}

static inline float
srgb_oetf (float v)
{
  if (fabsf (v) > 0.0031308f)
    return sign (v) * (1.055f * powf (fabsf (v), 1.f / 2.4f) - 0.055f);
  else
    return 12.92f * v;
}

static inline float
srgb_eotf (float v)
{
  if (fabsf (v) >= 0.04045f)
    return sign (v) * powf (((fabsf (v) + 0.055f) / (1.f + 0.055f)), 2.4f);
  else
    return v / 12.92f;
}

static inline float
gamma22_oetf (float v)
{
  return sign (v) * powf (fabsf (v), 1.f / 2.2f);
}

static inline float
gamma22_eotf (float v)
{
  return sign (v) * powf (fabsf (v), 2.2f);
}

static inline float
gamma28_oetf (float v)
{
  return sign (v) * powf (fabsf (v), 1.f / 2.8f);
}

static inline float
gamma28_eotf (float v)
{
  return sign (v) * powf (fabsf (v), 2.8f);
}

static inline float
pq_eotf (float v)
{
  float ninv = (1 << 14) / 2610.0;
  float minv = (1 << 5) / 2523.0;
  float c1 = 3424.0 / (1 << 12);
  float c2 = 2413.0 / (1 << 7);
  float c3 = 2392.0 / (1 << 7);

  float x = powf (fabsf (v), minv);
  x = powf (MAX ((x - c1), 0) / (c2 - (c3 * x)), ninv);

  return sign (v) * x * 10000 / 203.0;
}

static inline float
pq_oetf (float v)
{
  float x = v * 203.0 / 10000.0;
  float n = 2610.0 / (1 << 14);
  float m = 2523.0 / (1 << 5);
  float c1 = 3424.0 / (1 << 12);
  float c2 = 2413.0 / (1 << 7);
  float c3 = 2392.0 / (1 << 7);

  x = powf (fabsf (x), n);
  return sign (v) * powf (((c1 + (c2 * x)) / (1 + (c3 * x))), m);
}

static inline float
bt709_eotf (float v)
{
  const float a = 1.099;
  const float d = 0.0812;

  if (fabsf (v) < d)
    return  v / 4.5f;
  else
    return sign (v) * powf ((fabsf (v) + (a - 1)) / a, 1 / 0.45f);
}

static inline float
bt709_oetf (float v)
{
  const float a = 1.099;
  const float b = 0.018;

  if (fabsf (v) < b)
    return v * 4.5f;
  else
    return sign (v) * (a * powf (fabsf (v), 0.45f) - (a - 1));
}

static inline float
hlg_eotf (float v)
{
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;

  if (fabsf (v) <= 0.5)
    return sign (v) * (v * v) / 3;
  else
    return sign (v) * (expf ((fabsf (v) - c) / a) + b) / 12.0;
}

static inline float
hlg_oetf (float v)
{
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;

  if (fabsf (v) <= 1/12.0)
    return sign (v) * sqrtf (3 * fabsf (v));
  else
    return sign (v) * (a * logf (12 * fabsf (v) - b) + c);
}

static const float identity[9] = {
  1, 0, 0,
  0, 1, 0,
  0, 0, 1,
};

/* See https://registry.khronos.org/DataFormat/specs/1.3/dataformat.1.3.html#PRIMARY_CONVERSION
 * for how to derive the abc_to_xyz matrices from chromaticity coordinates.
 *
 * Code for this can be found in compute_to_xyz_from_primaries().
 */

static const float srgb_to_xyz[9] = {
  0.412391, 0.357584, 0.180481,
  0.212639, 0.715169, 0.072192,
  0.019331, 0.119195, 0.950532,
};

static const float xyz_to_srgb[9] = {
  3.240970, -1.537383, -0.498611,
 -0.969244,  1.875968,  0.041555,
  0.055630, -0.203977,  1.056972,
};

static const float pal_to_xyz[9] = {
  0.430554, 0.341550, 0.178352,
  0.222004, 0.706655, 0.071341,
  0.020182, 0.129553, 0.939322,
};
static const float xyz_to_pal[9] = {
  3.063360, -1.393389, -0.475824,
 -0.969244,  1.875968,  0.041555,
  0.067861, -0.228799,  1.069090,
};

static const float ntsc_to_xyz[9] = {
  0.393521, 0.365258, 0.191677,
  0.212376, 0.701060, 0.086564,
  0.018739, 0.111934, 0.958385,
};

static const float xyz_to_ntsc[9] = {
  3.506002, -1.739790, -0.544058,
 -1.069048,  1.977779,  0.035171,
  0.056307, -0.196976,  1.049953,
};

static const float rec2020_to_xyz[9] = {
  0.636958, 0.144617, 0.168881,
  0.262700, 0.677998, 0.059302,
  0.000000, 0.028073, 1.060985,
};

static const float xyz_to_rec2020[9] = {
  1.716651, -0.355671, -0.253366,
 -0.666684,  1.616481,  0.015769,
  0.017640, -0.042771,  0.942103,
};

static const float p3_to_xyz[9] = {
  0.486571, 0.265668, 0.198217,
  0.228975, 0.691739, 0.079287,
  0.000000, 0.045113, 1.043944,
};

static const float xyz_to_p3[9] = {
  2.493497, -0.931384, -0.402711,
 -0.829489,  1.762664,  0.023625,
  0.035846, -0.076172,  0.956885,
};

/* premultiplied matrices for default conversions */

static const float rec2020_to_srgb[9] = {
  1.660227, -0.587548, -0.072838,
 -0.124553,  1.132926, -0.008350,
 -0.018155, -0.100603,  1.118998,
};

static const float srgb_to_rec2020[9] = {
  0.627504, 0.329275, 0.043303,
  0.069108, 0.919519, 0.011360,
  0.016394, 0.088011, 0.895380,
};

/* luminances */

#define DEFAULT_SDR_LUMINANCE { .min = 0.2, .max = 80, .ref = 80 }
#define DEFAULT_HDR_LUMINANCE { .min = 0.005, .max = 10000, .ref = 203 }

static const GdkLuminance default_sdr_luminance = DEFAULT_SDR_LUMINANCE;
static const GdkLuminance default_hdr_luminance = DEFAULT_HDR_LUMINANCE;

/* primaries */

#define SRGB_PRIMARIES { \
  .rx = 0.640, .ry = 0.330, \
  .gx = 0.300, .gy = 0.600, \
  .bx = 0.150, .by = 0.060, \
  .wx = 0.3127, .wy = 0.3290 \
}

#define PAL_PRIMARIES { \
  .rx = 0.64, .ry = 0.33, \
  .gx = 0.29, .gy = 0.60, \
  .bx = 0.15, .by = 0.06, \
  .wx = 0.3127, .wy = 0.3290 \
}

#define NTSC_PRIMARIES { \
  .rx = 0.630, .ry = 0.340, \
  .gx = 0.310, .gy = 0.595, \
  .bx = 0.155, .by = 0.070, \
  .wx = 0.3127, .wy = 0.3290 \
}

#define REC2020_PRIMARIES { \
  .rx = 0.708, .ry = 0.292, \
  .gx = 0.170, .gy = 0.797, \
  .bx = 0.131, .by = 0.046, \
  .wx = 0.3127, .wy = 0.3290 \
}

#define P3_PRIMARIES { \
  .rx = 0.680, .ry = 0.320, \
  .gx = 0.265, .gy = 0.690, \
  .bx = 0.150, .by = 0.060, \
  .wx = 0.3127, .wy = 0.3290 \
}

static const GdkPrimaries srgb_primaries = SRGB_PRIMARIES;
static const GdkPrimaries pal_primaries = PAL_PRIMARIES;
static const GdkPrimaries ntsc_primaries = NTSC_PRIMARIES;
static const GdkPrimaries rec2020_primaries = REC2020_PRIMARIES;
static const GdkPrimaries p3_primaries = P3_PRIMARIES;

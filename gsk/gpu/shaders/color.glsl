#ifndef _COLOR_
#define _COLOR_

#define COLOR_SPACE_OUTPUT_PREMULTIPLIED (1u << 2)
#define COLOR_SPACE_ALT_PREMULTIPLIED    (1u << 3)
#define COLOR_SPACE_OUTPUT_SHIFT 8u
#define COLOR_SPACE_ALT_SHIFT 16u
#define COLOR_SPACE_COLOR_STATE_MASK 0xFFu

#define OUTPUT_COLOR_SPACE ((GSK_COLOR_STATES >> COLOR_SPACE_OUTPUT_SHIFT) & COLOR_SPACE_COLOR_STATE_MASK)
#define ALT_COLOR_SPACE ((GSK_COLOR_STATES >> COLOR_SPACE_ALT_SHIFT) & COLOR_SPACE_COLOR_STATE_MASK)
#define OUTPUT_PREMULTIPLIED ((GSK_COLOR_STATES & COLOR_SPACE_OUTPUT_PREMULTIPLIED) == COLOR_SPACE_OUTPUT_PREMULTIPLIED)
#define ALT_PREMULTIPLIED ((GSK_COLOR_STATES & COLOR_SPACE_ALT_PREMULTIPLIED) == COLOR_SPACE_ALT_PREMULTIPLIED)

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
alt_color_alpha (vec4  color,
                 float alpha)
{
  if (ALT_PREMULTIPLIED)
    return color * alpha;
  else
    return vec4 (color.rgb, color.a * alpha);
}

vec4
output_color_alpha (vec4  color,
                    float alpha)
{
  if (OUTPUT_PREMULTIPLIED)
    return color * alpha;
  else
    return vec4 (color.rgb, color.a * alpha);
}

vec3
srgb_eotf (vec3 v)
{
  vec3 lo = v / 12.92;
  vec3 hi = sign (v) * pow (((abs (v) + 0.055) / (1.0 + 0.055)), vec3 (2.4));
  return mix (lo, hi, greaterThanEqual (abs (v), vec3 (0.04045)));
}

vec3
srgb_oetf (vec3 v)
{
  vec3 lo = 12.92 * v;
  vec3 hi = sign (v) * (1.055 * pow (abs (v), vec3 (1.0 / 2.4)) - 0.055);
  return mix (lo, hi, greaterThan (abs (v), vec3 (0.0031308)));
}

vec3
pq_eotf (vec3 v)
{
  const float ninv = 16384.0 / 2610.0;
  const float minv = 32.0 / 2523.0;
  const float c1 = 3424.0 / 4096.0;
  const float c2 = 2413.0 / 128.0;
  const float c3 = 2392.0 / 128.0;

  vec3 x = pow (abs (v), vec3 (minv));
  x = pow (max (x - c1, 0.0) / (c2 - (c3 * x)), vec3 (ninv));

  return sign (v) * x;
}

vec3
pq_oetf (vec3 v)
{
  const float n = 2610.0 / 16384.0;
  const float m = 2523.0 / 32.0;
  const float c1 = 3424.0 / 4096.0;
  const float c2 = 2413.0 / 128.0;
  const float c3 = 2392.0 / 128.0;

  vec3 x = pow (abs (v), vec3 (n));

  return sign (v) * pow (((c1 + (c2 * x)) / (1.0 + (c3 * x))), vec3 (m));
}

/* Note that these matrices are transposed from the C version */

const mat3 identity = mat3(
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0
);

const mat3 srgb_from_rec2020 = mat3(
  1.660227, -0.124553, -0.018155,
 -0.587548,  1.132926, -0.100603,
 -0.072838, -0.008350, 1.118998
);

const mat3 rec2020_from_srgb = mat3(
  0.627504, 0.069108, 0.016394,
  0.329275, 0.919519, 0.088011,
  0.043303, 0.011360, 0.895380
);

const mat3 srgb_to_xyz = mat3(
  0.4124564, 0.2126729, 0.0193339,
  0.3575761, 0.7151522, 0.1191920,
  0.1804375, 0.0721750, 0.9503041
);

const mat3 xyz_to_srgb = mat3(
  3.2404542, -0.9692660,  0.0556434,
 -1.5371385,  1.8760108, -0.2040259,
 -0.4985314,  0.0415560,  1.0572252
);

const mat3 rec2020_to_xyz = mat3(
  0.6369580, 0.2627002, 0.0000000,
  0.1446169, 0.6779981, 0.0280727,
  0.1688810, 0.0593017, 1.0609851
);

const mat3 xyz_to_rec2020 = mat3(
  1.7166512, -0.6666844,  0.0176399,
 -0.3556708,  1.6164812, -0.0427706,
 -0.2533663,  0.0157685,  0.9421031
);

/* HPE LMS matrix with crosstalk 0.04 */
/* Ref: https://professional.dolby.com/siteassets/pdfs/ictcp_dolbywhitepaper_v071.pdf */
const mat3 xyz_to_lms = mat3(
  0.3592833157, -0.1920808171, 0.0070797916,
  0.6976050754,  1.1004767415, 0.0748396541,
 -0.0358915960,  0.0753748831, 0.8433265123
);

const mat3 lms_to_xyz = mat3(
  2.0701520550,  0.3647384571, -0.0497472147,
 -1.3263472277,  0.6805661141, -0.0492609565,
  0.2066510727, -0.0453045711,  1.1880659712
);

const mat3 lms_pq_to_ictcp = mat3(
  0.5,  1.613525390625,  4.378173828125,
  0.5, -3.323486328125, -4.24560546875,
  0.0,  1.709716796875, -0.132568359375
);

const mat3 ictcp_to_lms_pq = mat3(
  1.0,                1.0,                  1.0,
  0.008609037037933, -0.008609037037933,    0.560031335710679,
  0.111029625003026, -0.111029625003026,   -0.320627174987319
);

mat3
cs_to_xyz (uint cs)
{
  switch (cs)
    {
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return srgb_to_xyz;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return rec2020_to_xyz;
    default:
      return identity;
    }
}

mat3
cs_from_xyz (uint cs)
{
  switch (cs)
    {
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return xyz_to_srgb;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return xyz_to_rec2020;
    default:
      return identity;
    }
}

uint
luminance (uint cs)
{
  switch (cs)
    {
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return SDR_LUMINANCE;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return PQ_LUMINANCE;
    default:
      return SDR_LUMINANCE;
    }
}

float
max_nits (uint luminance)
{
  switch (luminance)
    {
    case SDR_LUMINANCE:  return 80.0;
    case PQ_LUMINANCE:   return 10000.0;
    case HLG_LUMINANCE:  return 1000.0;
    default:             return 80.0;
    }
}

float
ref_nits (uint luminance)
{
  switch (luminance)
    {
    case SDR_LUMINANCE:  return 80.0;
    case PQ_LUMINANCE:
    case HLG_LUMINANCE:  return 203.0;
    default:             return 80.0;
    }
}

vec3
ictcp_tone_map (vec3 color,
                mat3 to_xyz,
                mat3 from_xyz,
                uint from_lum,
                uint to_lum)
{
  float src_max = max_nits (from_lum);
  float src_ref = ref_nits (from_lum);
  float dst_max = max_nits (to_lum);
  float dst_ref = ref_nits (to_lum);

  float headroom = dst_max / dst_ref;
  float tm_ref = headroom >= 1.5 ? dst_ref : dst_max / 1.5;

  /* Forward: linear RGB -> XYZ -> LMS -> PQ -> ICtCp */
  vec3 lms = xyz_to_lms * (to_xyz * color);
  lms = pq_oetf (lms);
  vec3 ictcp = lms_pq_to_ictcp * lms;

  /* Tone curve on I in linear nits */
  float lum = pq_eotf (vec3 (ictcp.x, 0.0, 0.0)).x * src_max;

  if (lum < src_ref)
    {
      lum *= tm_ref / src_ref;
    }
  else
    {
      float ratio = (lum - src_ref) / (src_max - src_ref);
      lum = tm_ref + (dst_max - tm_ref) * 5.0 * ratio / (4.0 * ratio + 1.0);
    }

  ictcp.x = pq_oetf (vec3 (lum / dst_max, 0.0, 0.0)).x;

  /* Reverse: ICtCp -> PQ -> LMS -> XYZ -> linear RGB */
  lms = pq_eotf (ictcp_to_lms_pq * ictcp);
  color = from_xyz * (lms_to_xyz * lms);

  return color;
}

vec3
linear_tone_map (vec3 color,
                 uint from_luminance,
                 uint to_luminance)
{
  return color *
         (max_nits (from_luminance) / ref_nits (from_luminance)) *
         (ref_nits (to_luminance) / max_nits (to_luminance));
}

vec3
apply_tone_map (vec3 color,
                uint from,
                uint to)
{
  uint from_lum = luminance (from);
  uint to_lum = luminance (to);

  if (from_lum == to_lum)
    return color;

  return linear_tone_map (color, from_lum, to_lum);
}

vec3
apply_eotf (vec3 color,
            uint cs)
{
  switch (cs)
    {
    case GDK_COLOR_STATE_ID_SRGB:
      return srgb_eotf (color);

    case GDK_COLOR_STATE_ID_REC2100_PQ:
      return pq_eotf (color);

    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return color;

    default:
      return vec3(1.0, 0.0, 0.8);
    }
}

vec3
apply_oetf (vec3 color,
            uint cs)
{
  switch (cs)
    {
    case GDK_COLOR_STATE_ID_SRGB:
      return srgb_oetf (color);

    case GDK_COLOR_STATE_ID_REC2100_PQ:
      return pq_oetf (color);

    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return color;

    default:
      return vec3(0.0, 1.0, 0.8);
    }
}

vec3
convert_linear (vec3 color,
                uint from,
                uint to)
{
  if (to == GDK_COLOR_STATE_ID_REC2100_LINEAR && from == GDK_COLOR_STATE_ID_SRGB_LINEAR)
    return rec2020_from_srgb * color;
  else if (to == GDK_COLOR_STATE_ID_SRGB_LINEAR && from == GDK_COLOR_STATE_ID_REC2100_LINEAR)
    return srgb_from_rec2020 * color;
  else
    return vec3(0.8, 1.0, 0.0);
}

uint
linear_color_space (uint cs)
{
  switch (cs)
    {
    case GDK_COLOR_STATE_ID_SRGB:           return GDK_COLOR_STATE_ID_SRGB_LINEAR;
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:    return GDK_COLOR_STATE_ID_SRGB_LINEAR;
    case GDK_COLOR_STATE_ID_REC2100_PQ:     return GDK_COLOR_STATE_ID_REC2100_LINEAR;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR: return GDK_COLOR_STATE_ID_REC2100_LINEAR;
    default:                                return 0u;
  };
}

vec4
convert_color (vec4 color,
               uint from,
               bool from_premul,
               uint to,
               bool to_premul)
{
  if (from_premul && (!to_premul || from != to))
    color = color_unpremultiply (color);

  if (from != to)
    {
      uint from_linear = linear_color_space (from);
      uint to_linear = linear_color_space (to);

      if (from_linear != from)
        color.rgb = apply_eotf (color.rgb, from);

      if (from_linear != to_linear)
        {
          color.rgb = apply_tone_map (color.rgb, from_linear, to_linear);
          color.rgb = convert_linear (color.rgb, from_linear, to_linear);
        }

      if (to_linear != to)
        color.rgb = apply_oetf (color.rgb, to);
    }

  if (to_premul && (!from_premul || from != to))
    color = color_premultiply (color);

  return color;
}

vec4
alt_color_from_output (vec4 color)
{
  return convert_color (color,
                        OUTPUT_COLOR_SPACE, OUTPUT_PREMULTIPLIED,
                        ALT_COLOR_SPACE, ALT_PREMULTIPLIED);
}

vec4
output_color_from_alt (vec4 color)
{
  return convert_color (color,
                        ALT_COLOR_SPACE, ALT_PREMULTIPLIED,
                        OUTPUT_COLOR_SPACE, OUTPUT_PREMULTIPLIED);
}

#endif /* _COLOR_ */

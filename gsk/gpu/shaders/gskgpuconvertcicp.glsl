#define GSK_N_TEXTURES 1

#include "common.glsl"

#define VARIATION_OPACITY              (1u << 0)
#define VARIATION_STRAIGHT_ALPHA       (1u << 1)
#define VARIATION_REVERSE              (1u << 2)

#define HAS_VARIATION(var) ((GSK_VARIATION & var) == var)

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS(2) vec2 _tex_coord;
PASS_FLAT(3) float _opacity;
PASS_FLAT(4) uint _transfer_function;
PASS_FLAT(5) mat3 _mat;

#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_tex_rect;
IN(2) float in_opacity;
IN(3) uint in_color_primaries;
IN(4) uint in_transfer_function;


const mat3 identity = mat3(
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0
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

const mat3 pal_to_xyz = mat3(
 0.4305538, 0.2220043, 0.0201822,
 0.3415498, 0.7066548, 0.1295534,
 0.1783523, 0.0713409, 0.9393222
);

const mat3 xyz_to_pal = mat3(
  3.0633611, -0.9692436,  0.0678610,
 -1.3933902,  1.8759675, -0.2287993,
 -0.4758237,  0.0415551,  1.0690896
);

const mat3 ntsc_to_xyz = mat3(
 0.3935209, 0.2123764, 0.0187391,
 0.3652581, 0.7010599, 0.1119339,
 0.1916769, 0.0865638, 0.9583847
);

const mat3 xyz_to_ntsc = mat3(
  3.5060033, -1.0690476,  0.0563066,
 -1.7397907,  1.9777789, -0.1969757,
 -0.5440583,  0.0351714,  1.0499523
);

const mat3 p3_to_xyz = mat3(
 0.4865709, 0.2289746, 0.0000000,
 0.2656677, 0.6917385, 0.0451134,
 0.1982173, 0.0792869, 1.0439444
);

const mat3 xyz_to_p3 = mat3(
  2.4934969, -0.8294890,  0.0358458,
 -0.9313836,  1.7626641, -0.0761724,
 -0.4027108,  0.0236247,  0.9568845
);

mat3
cicp_to_xyz (uint cp)
{
  switch (cp)
    {
    case 1u: return srgb_to_xyz;
    case 5u: return pal_to_xyz;
    case 6u: return ntsc_to_xyz;
    case 9u: return rec2020_to_xyz;
    case 10u: return identity;
    case 12u: return p3_to_xyz;
    default: return identity;
    }
}

mat3
cicp_from_xyz (uint cp)
{
  switch (cp)
    {
    case 1u: return xyz_to_srgb;
    case 5u: return xyz_to_pal;
    case 6u: return xyz_to_ntsc;
    case 9u: return xyz_to_rec2020;
    case 10u: return identity;
    case 12u: return xyz_to_p3;
    default: return identity;
    }
}


void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);

  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
  _opacity = in_opacity;
  _transfer_function = in_transfer_function;

  if (HAS_VARIATION (VARIATION_REVERSE))
    {
      if (OUTPUT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB ||
          OUTPUT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB_LINEAR)
        _mat = cicp_from_xyz (in_color_primaries) * srgb_to_xyz;
      else
        _mat = cicp_from_xyz (in_color_primaries) * rec2020_to_xyz;
    }
  else
    {
      if (OUTPUT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB ||
          OUTPUT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB_LINEAR)
        _mat = xyz_to_srgb * cicp_to_xyz (in_color_primaries);
      else
        _mat = xyz_to_rec2020 * cicp_to_xyz (in_color_primaries);
    }
}

#endif


#ifdef GSK_FRAGMENT_SHADER


float
bt709_eotf (float v)
{
  const float a = 1.099;
  const float d = 0.0812;

  if (abs (v) < d)
    return v / 4.5;
  else
    return sign (v) * pow ((abs (v) + (a - 1.0)) / a, 1.0 / 0.45);
}

float
bt709_oetf (float v)
{
  const float a = 1.099;
  const float b = 0.018;

  if (abs (v) < b)
    return v * 4.5;
  else
    return sign (v) * (a * pow (abs (v), 0.45) - (a - 1.0));
}

float
gamma22_oetf (float v)
{
  return sign (v) * pow (abs (v), 1.0 / 2.2);
}

float
gamma22_eotf (float v)
{
  return sign (v) * pow (abs (v), 2.2);
}

float
gamma28_oetf (float v)
{
  return sign (v) * pow (abs (v), 1.0 / 2.8);
}

float
gamma28_eotf (float v)
{
  return sign (v) * pow (abs (v), 2.8);
}

float
hlg_eotf (float v)
{
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;

  if (abs (v) <= 0.5)
    return sign (v) * ((v * v) / 3.0);
  else
    return sign (v) * (exp (((abs (v) - c) / a) + b) / 12.0);
}

float
hlg_oetf (float v)
{
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;

  if (abs (v) <= 1.0 / 12.0)
    return sign (v) * sqrt (3.0 * abs (v));
  else
    return sign (v) * (a * log (12.0 * abs (v) - b) + c);
}

vec3
apply_cicp_eotf (vec3 color,
                 uint transfer_function)
{
  switch (transfer_function)
    {
    case 1u:
    case 6u:
    case 14u:
    case 15u:
      return vec3 (bt709_eotf (color.r),
                   bt709_eotf (color.g),
                   bt709_eotf (color.b));

    case 4u:
      return vec3 (gamma22_eotf (color.r),
                   gamma22_eotf (color.g),
                   gamma22_eotf (color.b));

    case 5u:
      return vec3 (gamma28_eotf (color.r),
                   gamma28_eotf (color.g),
                   gamma28_eotf (color.b));

    case 8u:
      return color;

    case 13u:
      return vec3 (srgb_eotf (color.r),
                   srgb_eotf (color.g),
                   srgb_eotf (color.b));

    case 16u:
      return vec3 (pq_eotf (color.r),
                   pq_eotf (color.g),
                   pq_eotf (color.b));

    case 18u:
      return vec3 (hlg_eotf (color.r),
                   hlg_eotf (color.g),
                   hlg_eotf (color.b));

    default:
      return vec3 (1.0, 0.2, 0.8);
    }
}

vec3
apply_cicp_oetf (vec3 color,
                 uint transfer_function)
{
  switch (transfer_function)
    {
    case 1u:
    case 6u:
    case 14u:
    case 15u:
      return vec3 (bt709_oetf (color.r),
                   bt709_oetf (color.g),
                   bt709_oetf (color.b));

    case 4u:
      return vec3 (gamma22_oetf (color.r),
                   gamma22_oetf (color.g),
                   gamma22_oetf (color.b));

    case 5u:
      return vec3 (gamma28_oetf (color.r),
                   gamma28_oetf (color.g),
                   gamma28_oetf (color.b));

    case 8u:
      return color;

    case 13u:
      return vec3 (srgb_oetf (color.r),
                   srgb_oetf (color.g),
                   srgb_oetf (color.b));

    case 16u:
      return vec3 (pq_oetf (color.r),
                   pq_oetf (color.g),
                   pq_oetf (color.b));

    case 18u:
      return vec3 (hlg_oetf (color.r),
                   hlg_oetf (color.g),
                   hlg_oetf (color.b));

    default:
      return vec3 (1.0, 0.2, 0.8);
    }
}

vec4
convert_color_from_cicp (vec4 color,
                         bool from_premul,
                         uint to,
                         bool to_premul)
{
  if (from_premul)
    color = color_unpremultiply (color);

  color.rgb = apply_cicp_eotf (color.rgb, _transfer_function);
  color.rgb = _mat * color.rgb;
  color.rgb = apply_oetf (color.rgb, to);

  if (to_premul)
    color = color_premultiply (color);

  return color;
}

vec4
convert_color_to_cicp (vec4 color,
                       bool to_premul,
                       uint from,
                       bool from_premul)
{
  if (from_premul)
    color = color_unpremultiply (color);

  color.rgb = apply_eotf (color.rgb, from);
  color.rgb = _mat * color.rgb;
  color.rgb = apply_cicp_oetf (color.rgb, _transfer_function);

  if (to_premul)
    color = color_premultiply (color);

  return color;
}

void
run (out vec4 color,
     out vec2 position)
{
  vec4 pixel;

  if (HAS_VARIATION (VARIATION_STRAIGHT_ALPHA))
    pixel = gsk_texture_straight_alpha (GSK_TEXTURE0, _tex_coord);
  else
    pixel = texture (GSK_TEXTURE0, _tex_coord);

  if (HAS_VARIATION (VARIATION_REVERSE))
    pixel = convert_color_to_cicp (pixel,
                                   ALT_PREMULTIPLIED,
                                   OUTPUT_COLOR_SPACE, OUTPUT_PREMULTIPLIED);
  else
    pixel = convert_color_from_cicp (pixel,
                                     ALT_PREMULTIPLIED,
                                     OUTPUT_COLOR_SPACE, OUTPUT_PREMULTIPLIED);

  float alpha = rect_coverage (_rect, _pos);
  if (HAS_VARIATION (VARIATION_OPACITY))
    alpha *= _opacity;

  color = output_color_alpha (pixel, alpha);

  position = _pos;
}

#endif

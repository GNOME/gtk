#define GSK_N_TEXTURES 1

#define NEED_OTHER_TF
#define NEED_OTHER_MATRICES
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

mat3
cicp_to_xyz (uint cp)
{
  switch (cp)
    {
    case 1: return srgb_to_xyz;
    case 5: return pal_to_xyz;
    case 6: return ntsc_to_xyz;
    case 9: return rec2020_to_xyz;
    case 10: return identity;
    case 12: return p3_to_xyz;
    default: return identity;
    }
}

mat3
cicp_from_xyz (uint cp)
{
  switch (cp)
    {
    case 1: return xyz_to_srgb;
    case 5: return xyz_to_pal;
    case 6: return xyz_to_ntsc;
    case 9: return xyz_to_rec2020;
    case 10: return identity;
    case 12: return xyz_to_p3;
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

vec3
apply_cicp_eotf (vec3 color,
                 uint transfer_function)
{
  switch (transfer_function)
    {
    case 1:
    case 6:
    case 14:
    case 15:
      return vec3 (bt709_eotf (color.r),
                   bt709_eotf (color.g),
                   bt709_eotf (color.b));

    case 8:
      return color;

    case 13:
      return vec3 (srgb_eotf (color.r),
                   srgb_eotf (color.g),
                   srgb_eotf (color.b));

    case 16:
      return vec3 (pq_eotf (color.r),
                   pq_eotf (color.g),
                   pq_eotf (color.b));

    case 18:
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
    case 1:
    case 6:
    case 14:
    case 15:
      return vec3 (bt709_oetf (color.r),
                   bt709_oetf (color.g),
                   bt709_oetf (color.b));

    case 8:
      return color;

    case 13:
      return vec3 (srgb_oetf (color.r),
                   srgb_oetf (color.g),
                   srgb_oetf (color.b));

    case 16:
      return vec3 (pq_oetf (color.r),
                   pq_oetf (color.g),
                   pq_oetf (color.b));

    case 18:
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
    pixel = convert_color_to_cicp (color,
                                   ALT_PREMULTIPLIED,
                                   OUTPUT_COLOR_SPACE, OUTPUT_PREMULTIPLIED);
  else
    pixel = convert_color_from_cicp (color,
                                     ALT_PREMULTIPLIED,
                                     OUTPUT_COLOR_SPACE, OUTPUT_PREMULTIPLIED);

  float alpha = rect_coverage (_rect, _pos);
  if (HAS_VARIATION (VARIATION_OPACITY))
    alpha *= _opacity;

  color = output_color_alpha (pixel, alpha);

  position = _pos;
}

#endif

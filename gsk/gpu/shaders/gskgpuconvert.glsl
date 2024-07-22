#define GSK_N_TEXTURES 1

#include "common.glsl"

#define VARIATION_OPACITY              (1u << 0)
#define VARIATION_STRAIGHT_ALPHA       (1u << 1)

#define HAS_VARIATION(var) ((GSK_VARIATION & var) == var)

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

void
run (out vec4 color,
     out vec2 position)
{
  vec4 pixel;
  if (HAS_VARIATION (VARIATION_STRAIGHT_ALPHA))
    pixel = gsk_texture_straight_alpha (GSK_TEXTURE0, _tex_coord);
  else
    pixel = texture (GSK_TEXTURE0, _tex_coord);

  pixel = output_color_from_alt (pixel);

  float alpha = rect_coverage (_rect, _pos);
  if (HAS_VARIATION (VARIATION_OPACITY))
    alpha *= _opacity;

  color = output_color_alpha (pixel, alpha);

  position = _pos;
}

#endif

#ifdef GSK_PREAMBLE
textures = 1;
ccs_premultiplied = argument;
acs_premultiplied = true;

graphene_rect_t rect;
graphene_rect_t tex_rect;
float opacity;

variation: gboolean opacity;
#endif

#include "gskgpuconvertinstance.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS(2) vec2 _tex_coord;
PASS_FLAT(3) float _opacity;

#ifdef GSK_VERTEX_SHADER

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
  vec4 pixel = gsk_texture0 (_tex_coord);

  pixel = output_color_from_alt (pixel);

  float alpha = rect_coverage (_rect, _pos);
  if (VARIATION_OPACITY)
    alpha *= _opacity;

  color = output_color_alpha (pixel, alpha);

  position = _pos;
}

#endif

#define GSK_N_TEXTURES 2

#include "common.glsl"
#include "blendmode.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _bottom_rect;
PASS_FLAT(2) Rect _top_rect;
PASS(3) vec2 _bottom_coord;
PASS(4) vec2 _top_coord;
PASS_FLAT(5) float _opacity;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_bottom_rect;
IN(2) vec4 in_top_rect;
IN(3) float in_opacity;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _opacity = in_opacity;

  Rect bottom_rect = rect_from_gsk (in_bottom_rect);
  _bottom_rect = bottom_rect;
  _bottom_coord = rect_get_coord (bottom_rect, pos);

  Rect top_rect = rect_from_gsk (in_top_rect);
  _top_rect = top_rect;
  _top_coord = rect_get_coord (top_rect, pos);
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 bottom_color = texture (GSK_TEXTURE0, _bottom_coord);
  bottom_color = output_color_alpha (bottom_color, rect_coverage (_bottom_rect, _pos));

  vec4 top_color = texture (GSK_TEXTURE1, _top_coord);
  top_color = output_color_alpha (top_color, rect_coverage (_top_rect, _pos));

  color = blend_mode (bottom_color, top_color, GSK_VARIATION);
  color = output_color_alpha (color, _opacity);

  position = _pos;
}

#endif

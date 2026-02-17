#ifdef GSK_PREAMBLE
graphene_rect_t bounds;
GdkColor color;
#endif /* GSK_PREAMBLE */

#include "gskgpucolorinstance.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _bounds;
PASS_FLAT(2) vec4 _color;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect b = rect_from_gsk (in_bounds);
  
  pos = rect_get_position (b);

  _pos = pos;
  _bounds = b;
  _color = output_color_from_alt (in_color);
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  color = output_color_alpha (_color, rect_coverage (_bounds, _pos));
  position = _pos;
}

#endif

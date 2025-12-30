#ifdef GSK_PREAMBLE
textures = 1;

graphene_rect_t rect;
graphene_rect_t tex_rect;
#endif

#include "gskgputextureinstance.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS(2) vec2 _tex_coord;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  color = texture (GSK_TEXTURE0, _tex_coord) *
          rect_coverage (_rect, _pos);
  position = _pos;
}

#endif

#ifdef GSK_PREAMBLE
textures = 2;

graphene_rect_t rect;
graphene_rect_t first_rect;
graphene_rect_t second_rect;
graphene_vec4_t factors;
float opacity;
#endif

#include "gskgpuarithmeticinstance.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _first_rect;
PASS_FLAT(2) Rect _second_rect;
PASS(3) vec2 _first_coord;
PASS(4) vec2 _second_coord;
PASS_FLAT(5) float _opacity;
PASS_FLAT(6) vec4 _factors;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _opacity = in_opacity;
  _factors = in_factors;

  Rect first_rect = rect_from_gsk (in_first_rect);
  _first_rect = first_rect;
  _first_coord = rect_get_coord (first_rect, pos);

  Rect second_rect = rect_from_gsk (in_second_rect);
  _second_rect = second_rect;
  _second_coord = rect_get_coord (second_rect, pos);
}

#endif

vec4
arithmetic (vec4 i1, vec4 i2, vec4 k)
{
  return k.x * i1 * i2 + k.y * i1 + k.z * i2 + k.w;
}

#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 first_color = texture (GSK_TEXTURE0, _first_coord);
  first_color = output_color_alpha (first_color, rect_coverage (_first_rect, _pos));

  vec4 second_color = texture (GSK_TEXTURE1, _second_coord);
  second_color = output_color_alpha (second_color, rect_coverage (_second_rect, _pos));

  first_color = color_unpremultiply (first_color);
  second_color = color_unpremultiply (second_color);

  color = arithmetic (first_color, second_color, _factors);

  color = clamp (color, 0.0, 1.0);

  color = color_premultiply (color);

  color = output_color_alpha (color, _opacity);

  position = _pos;
}

#endif

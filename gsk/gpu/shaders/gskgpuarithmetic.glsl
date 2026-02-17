#ifdef GSK_PREAMBLE
textures = 2;
acs_equals_ccs = false;
acs_premultiplied = true;

graphene_rect_t bounds;
graphene_rect_t first_rect;
graphene_rect_t second_rect;
float k1;
float k2;
float k3;
float k4;
float opacity;
#endif /* GSK_PREAMBLE */

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
  Rect b = rect_from_gsk (in_bounds);
  
  pos = rect_get_position (b);

  _pos = pos;
  _opacity = in_opacity;
  _factors = vec4(in_k1, in_k2, in_k3, in_k4);

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
  vec4 result = k.x * i1 * i2 + k.y * i1 + k.z * i2 + k.w;
  return clamp (result, vec4 (0.0), vec4 (min (1.0, result.a)));
}

#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 first_color = texture (GSK_TEXTURE0, _first_coord);
  first_color = alt_color_from_output (first_color);
  first_color = alt_color_alpha (first_color, rect_coverage (_first_rect, _pos));

  vec4 second_color = texture (GSK_TEXTURE1, _second_coord);
  second_color = alt_color_from_output (second_color);
  second_color = alt_color_alpha (second_color, rect_coverage (_second_rect, _pos));

  color = arithmetic (first_color, second_color, _factors);

  color = output_color_from_alt (color);
  color = output_color_alpha (color, _opacity);

  position = _pos;
}

#endif

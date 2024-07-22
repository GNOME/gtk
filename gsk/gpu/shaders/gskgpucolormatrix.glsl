#define GSK_N_TEXTURES 1

#include "common.glsl"

PASS_FLAT(0) mat4 _color_matrix;
PASS_FLAT(4) vec4 _color_offset;
PASS(5) vec2 _pos;
PASS_FLAT(6) Rect _rect;
PASS(7) vec2 _tex_coord;


#ifdef GSK_VERTEX_SHADER

IN(0) mat4 in_color_matrix;
IN(4) vec4 in_color_offset;
IN(5) vec4 in_rect;
IN(6) vec4 in_tex_rect;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
  _color_matrix = in_color_matrix;
  _color_offset = in_color_offset;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 pixel = texture (GSK_TEXTURE0, _tex_coord);
  pixel = alt_color_from_output (pixel);

  pixel = _color_matrix * pixel + _color_offset;
  pixel = clamp (pixel, 0.0, 1.0);

  pixel = output_color_from_alt (pixel);

  color = output_color_alpha (pixel, rect_coverage (_rect, _pos));
  position = _pos;
}

#endif


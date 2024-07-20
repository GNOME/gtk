#define GSK_N_TEXTURES 2

#include "common.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _start_rect;
PASS_FLAT(2) Rect _end_rect;
PASS(3) vec2 _start_coord;
PASS(4) vec2 _end_coord;
PASS_FLAT(5) float _start_opacity;
PASS_FLAT(6) float _end_opacity;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_start_rect;
IN(2) vec4 in_end_rect;
IN(3) vec2 in_opacity_progress;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;

  Rect start_rect = rect_from_gsk (in_start_rect);
  _start_rect = start_rect;
  _start_coord = rect_get_coord (start_rect, pos);
  _start_opacity = in_opacity_progress[0] * (1.0 - in_opacity_progress[1]);

  Rect end_rect = rect_from_gsk (in_end_rect);
  _end_rect = end_rect;
  _end_coord = rect_get_coord (end_rect, pos);
  _end_opacity = in_opacity_progress[0] * in_opacity_progress[1];
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 start = output_color_alpha (texture (GSK_TEXTURE0, _start_coord),
                                   rect_coverage (_start_rect, _pos) *
                                   _start_opacity);
  vec4 end = output_color_alpha (texture (GSK_TEXTURE1, _end_coord),
                                 rect_coverage (_end_rect, _pos) *
                                 _end_opacity);

  color = start + end;
  position = _pos;
}

#endif

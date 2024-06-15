#include "common.glsl"

#define VARIATION_CONVERSIONS (GSK_VARIATION >> 2)

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _start_rect;
PASS_FLAT(2) Rect _end_rect;
PASS(3) vec2 _start_coord;
PASS(4) vec2 _end_coord;
PASS_FLAT(5) uint _start_id;
PASS_FLAT(6) uint _end_id;
PASS_FLAT(7) float _start_opacity;
PASS_FLAT(8) float _end_opacity;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_start_rect;
IN(2) uint in_start_id;
IN(3) vec4 in_end_rect;
IN(4) uint in_end_id;
IN(5) vec2 in_opacity_progress;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;

  Rect start_rect = rect_from_gsk (in_start_rect);
  _start_rect = start_rect;
  _start_coord = rect_get_coord (start_rect, pos);
  _start_id = in_start_id;
  _start_opacity = in_opacity_progress[0] * (1.0 - in_opacity_progress[1]);

  Rect end_rect = rect_from_gsk (in_end_rect);
  _end_rect = end_rect;
  _end_coord = rect_get_coord (end_rect, pos);
  _end_id = in_end_id;
  _end_opacity = in_opacity_progress[0] * in_opacity_progress[1];
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 start = gsk_texture (_start_id, _start_coord);
  vec4 end = gsk_texture (_end_id, _end_coord);

  if (VARIATION_CONVERSIONS != 0u)
    {
      uint start_color_state = VARIATION_CONVERSIONS & 31u;
      uint end_color_state = (VARIATION_CONVERSIONS >> 5) & 31u;
      uint target_color_state = (VARIATION_CONVERSIONS >> 10) & 31u;

      if (start_color_state != 0u && end_color_state != 0u &&
          start_color_state != end_color_state)
        {
          uint conversion = start_color_state | (end_color_state << 16);

          start = color_unpremultiply (start);
          start = color_convert (start, conversion);
          start = color_premultiply (start);
        }

      if (end_color_state != 0u && target_color_state != 0u &&
          end_color_state != target_color_state)
        {
          uint conversion = end_color_state | (target_color_state << 16);

          end = color_unpremultiply (end);
          end = color_convert (end, conversion);
          end = color_premultiply (end);
        }
    }

  start = start * rect_coverage (_start_rect, _pos);
  end = end * rect_coverage (_end_rect, _pos);

  color = start * _start_opacity + end * _end_opacity;

  position = _pos;
}

#endif

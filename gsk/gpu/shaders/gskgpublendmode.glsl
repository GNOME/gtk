#include "common.glsl"
#include "blendmode.glsl"

#define VARIATION_BLENDMODE (GSK_VARIATION & 0xff)
#define VARIATION_CONVERSIONS (GSK_VARIATION >> 8)

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _bottom_rect;
PASS_FLAT(2) Rect _top_rect;
PASS(3) vec2 _bottom_coord;
PASS(4) vec2 _top_coord;
PASS_FLAT(5) uint _bottom_id;
PASS_FLAT(6) uint _top_id;
PASS_FLAT(7) float _opacity;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_bottom_rect;
IN(2) uint in_bottom_id;
IN(3) vec4 in_top_rect;
IN(4) uint in_top_id;
IN(5) float in_opacity;

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
  _bottom_id = in_bottom_id;

  Rect top_rect = rect_from_gsk (in_top_rect);
  _top_rect = top_rect;
  _top_coord = rect_get_coord (top_rect, pos);
  _top_id = in_top_id;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 bottom = gsk_texture (_bottom_id, _bottom_coord);
  vec4 top = gsk_texture (_top_id, _top_coord);

  if (VARIATION_CONVERSIONS != 0u)
    {
      uint bottom_color_state = VARIATION_CONVERSIONS & 31u;
      uint top_color_state = (VARIATION_CONVERSIONS >> 5) & 31u;
      uint target_color_state = (VARIATION_CONVERSIONS >> 10) & 31u;

      if (bottom_color_state != 0u && target_color_state != 0u &&
          bottom_color_state != target_color_state)
        {
          uint conversion = bottom_color_state | (target_color_state << 16);

          bottom = color_unpremultiply (bottom);
          bottom = color_convert (bottom, conversion);
          bottom = color_premultiply (bottom);
        }

      if (top_color_state != 0u && target_color_state != 0u &&
          top_color_state != target_color_state)
        {
          uint conversion = top_color_state | (target_color_state << 16);

          top = color_unpremultiply (top);
          top = color_convert (top, conversion);
          top = color_premultiply (top);
        }
    }

  bottom = bottom * rect_coverage (_bottom_rect, _pos);
  top = top * rect_coverage (_top_rect, _pos);

  color = blend_mode (bottom, top, VARIATION_BLENDMODE) * _opacity;

  position = _pos;
}

#endif

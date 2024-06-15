#include "common.glsl"

#define VARIATION_MASK_MODE (GSK_VARIATION & 3u)

#define VARIATION_CONVERSIONS (GSK_VARIATION >> 2)

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _source_rect;
PASS_FLAT(2) Rect _mask_rect;
PASS(3) vec2 _source_coord;
PASS(4) vec2 _mask_coord;
PASS_FLAT(5) uint _source_id;
PASS_FLAT(6) uint _mask_id;
PASS_FLAT(7) float _opacity;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_source_rect;
IN(2) uint in_source_id;
IN(3) vec4 in_mask_rect;
IN(4) uint in_mask_id;
IN(5) float in_opacity;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);

  pos = rect_get_position (r);

  _pos = pos;
  Rect source_rect = rect_from_gsk (in_source_rect);
  _source_rect = source_rect;
  _source_coord = rect_get_coord (source_rect, pos);
  _source_id = in_source_id;
  Rect mask_rect = rect_from_gsk (in_mask_rect);
  _mask_rect = mask_rect;
  _mask_coord = rect_get_coord (mask_rect, pos);
  _mask_id = in_mask_id;
  _opacity = in_opacity;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 source = gsk_texture (_source_id, _source_coord);
  vec4 mask = gsk_texture (_mask_id, _mask_coord);

  if (VARIATION_CONVERSIONS != 0u)
    {
      uint source_color_state = VARIATION_CONVERSIONS & 31u;
      uint mask_color_state = (VARIATION_CONVERSIONS >> 5) & 31u;
      uint target_color_state = (VARIATION_CONVERSIONS >> 10) & 31u;

      if (source_color_state != 0u && target_color_state != 0u &&
          source_color_state != target_color_state)
        {
          uint conversion = source_color_state | (target_color_state << 16);

          source = color_unpremultiply (source);
          source = color_convert (source, conversion);
          source = color_premultiply (source);
        }

      if (mask_color_state != 0u && target_color_state != 0u &&
          mask_color_state != target_color_state)
        {
          uint conversion = mask_color_state | (target_color_state << 16);

          mask = color_unpremultiply (mask);
          mask = color_convert (mask, conversion);
          mask = color_premultiply (source);
        }
    }

  source = source * rect_coverage (_source_rect, _pos);
  mask =  mask * rect_coverage (_mask_rect, _pos);

  float alpha = _opacity;
  switch (VARIATION_MASK_MODE)
  {
    case GSK_MASK_MODE_ALPHA:
      alpha *= mask.a;
      break;

    case GSK_MASK_MODE_INVERTED_ALPHA:
      alpha *= (1.0 - mask.a);
      break;

    case GSK_MASK_MODE_LUMINANCE:
      alpha *= luminance (mask.rgb);
      break;

    case GSK_MASK_MODE_INVERTED_LUMINANCE:
      alpha *= (mask.a - luminance (mask.rgb));
      break;

    default:
      break;
  }

  color = source * alpha;
  position = _pos;
}

#endif

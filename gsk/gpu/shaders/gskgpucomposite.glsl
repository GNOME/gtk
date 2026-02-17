#ifdef GSK_PREAMBLE
textures = 2;
dual_blend = true;
acs_equals_ccs = true;
acs_premultiplied = true;

graphene_rect_t bounds;
graphene_rect_t source_rect;
graphene_rect_t mask_rect;
float opacity;

variation: GskPorterDuff operator;
#endif /* GSK_PREAMBLE */

#include "gskgpucompositeinstance.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _source_rect;
PASS_FLAT(2) Rect _mask_rect;
PASS(3) vec2 _source_coord;
PASS(4) vec2 _mask_coord;
PASS_FLAT(7) float _opacity;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect b = rect_from_gsk (in_bounds);
  
  pos = rect_get_position (b);

  _pos = pos;
  Rect source_rect = rect_from_gsk (in_source_rect);
  _source_rect = source_rect;
  _source_coord = rect_get_coord (source_rect, pos);
  Rect mask_rect = rect_from_gsk (in_mask_rect);
  _mask_rect = mask_rect;
  _mask_coord = rect_get_coord (mask_rect, pos);
  _opacity = in_opacity;
}

#endif


#ifdef GSK_FRAGMENT_SHADER

float
get_dest_alpha (float source,
                float mask)
{
  switch (VARIATION_OPERATOR)
  {
    case GSK_PORTER_DUFF_SOURCE:
    case GSK_PORTER_DUFF_SOURCE_IN_DEST:
    case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
    case GSK_PORTER_DUFF_CLEAR:
      return 1.0 - mask;
    case GSK_PORTER_DUFF_DEST:
    case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
      return 1.0;
    case GSK_PORTER_DUFF_SOURCE_OVER_DEST:
    case GSK_PORTER_DUFF_DEST_OUT_SOURCE:
    case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
    case GSK_PORTER_DUFF_XOR:
      return (1.0 - mask) + (1.0 - source) * mask;
    case GSK_PORTER_DUFF_DEST_IN_SOURCE:
    case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:
      return 1.0 - mask + source * mask;
    default:
      return 0.0;
  }
}

float
get_source_alpha (float mask)
{
  switch (VARIATION_OPERATOR)
  {
    case GSK_PORTER_DUFF_SOURCE:
    case GSK_PORTER_DUFF_SOURCE_OVER_DEST:
      return mask;
    case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
    case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
    case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:
    case GSK_PORTER_DUFF_XOR:
      return mask; /* ONE_MINUS_DST_ALPHA */
    case GSK_PORTER_DUFF_SOURCE_IN_DEST:
    case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
      return mask; /* DST_ALPHA */
    case GSK_PORTER_DUFF_DEST:
    case GSK_PORTER_DUFF_DEST_IN_SOURCE:
    case GSK_PORTER_DUFF_DEST_OUT_SOURCE:
    case GSK_PORTER_DUFF_CLEAR:
      return 0.0;
    default:
      return 0.0;
  }
}

void
run (out vec4 color,
     out vec4 mask_color,
     out vec2 position)
{
  vec4 source = texture (GSK_TEXTURE0, _source_coord);
  source = output_color_alpha (source, rect_coverage (_source_rect, _pos));
  vec4 mask = texture (GSK_TEXTURE1, _mask_coord);
  float alpha = _opacity * mask.a;

  mask_color = vec4 (get_dest_alpha (source.a, alpha));
  color = output_color_alpha (source, get_source_alpha (alpha));
  position = _pos;
}

#endif

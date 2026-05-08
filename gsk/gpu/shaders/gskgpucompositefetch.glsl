#ifdef GSK_PREAMBLE
textures = 2;
var_name = "gsk_gpu_composite_fetch";
struct_name = "GskGpuCompositeFetch";
acs_equals_ccs = true;
acs_premultiplied = true;

graphene_rect_t bounds;
graphene_rect_t source_rect;
graphene_rect_t mask_rect;
float opacity;

variation: GskPorterDuff operator;
#endif /* GSK_PREAMBLE */

#ifdef GSK_FRAGMENT_SHADER
#ifndef VULKAN
#extension GL_EXT_shader_framebuffer_fetch : require
#define GSK_FRAMEBUFFER_FETCH 1
#endif
#endif

#include "gskgpucompositefetchinstance.glsl"
#include "blendmode.glsl"

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

#endif /* VULKAN */

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
get_source_factor (float dest_alpha)
{
  switch (VARIATION_OPERATOR)
  {
    case GSK_PORTER_DUFF_SOURCE:
    case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
      return 1.0;
    case GSK_PORTER_DUFF_SOURCE_IN_DEST:
    case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
      return dest_alpha;
    case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
    case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:
    case GSK_PORTER_DUFF_XOR:
      return 1.0 - dest_alpha;
    default:
      return 1.0;
  }
}

#ifdef VULKAN

void
run (out vec4 color,
     out vec2 position)
{
  vec4 source = texture (GSK_TEXTURE0, _source_coord);
  source = alt_color_from_output (source);
  source = alt_color_alpha (source, rect_coverage (_source_rect, _pos));

  vec4 top = texture (GSK_TEXTURE1, _mask_coord);
  top = alt_color_from_output (top);
  top = alt_color_alpha (top, rect_coverage (_mask_rect, _pos));

  color = blend_mode (source, top, VARIATION_OPERATOR);
  color = output_color_from_alt (color);
  color = output_color_alpha (color, _opacity);
  position = _pos;
}

#endif /* VULKAN */

#ifndef VULKAN
void
run (out vec4 color,
     out vec2 position)
{
  vec4 source = texture (GSK_TEXTURE0, _source_coord);
  source = alt_color_from_output (source);
  source = alt_color_alpha (source, rect_coverage (_source_rect, _pos));

  vec4 mask = texture (GSK_TEXTURE1, _mask_coord);
  float alpha = _opacity * mask.a;

  vec4 dst = alt_color_from_output (out_color);

  vec4 result = output_color_alpha (source, alpha);
  result = result * get_source_factor (dst.a)
         + dst * get_dest_alpha (source.a, alpha);

  color = output_color_from_alt (result);
  color = output_color_alpha (color, _opacity);
  position = _pos;
}

#endif

#endif

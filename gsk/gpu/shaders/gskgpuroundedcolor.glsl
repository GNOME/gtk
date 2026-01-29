#ifdef GSK_PREAMBLE
var_name = "gsk_gpu_rounded_color";
struct_name = "GskGpuRoundedColor";

GskRoundedRect outline;
GdkColor color;
#endif /* GSK_PREAMBLE */

#include "gskgpuroundedcolorinstance.glsl"

#include "common.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) vec4 _color;
PASS_FLAT(2) RoundedRect _outline;

#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  RoundedRect outline = rounded_rect_from_gsk (in_outline);
  
  pos = rect_get_position (Rect (rounded_rect_bounds (outline)));

  _pos = pos;
  _outline= outline;
  _color = output_color_from_alt (in_color);
}

#endif

#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  color = output_color_alpha (_color, rounded_rect_coverage (_outline, _pos));
  position = _pos;
}

#endif

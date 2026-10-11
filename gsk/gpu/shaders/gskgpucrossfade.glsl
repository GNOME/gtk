#ifdef GSK_PREAMBLE
textures = 2;
var_name = "gsk_gpu_cross_fade";
struct_name = "GskGpuCrossFade";
acs_equals_ccs = true;
acs_premultiplied = true;

graphene_rect_t bounds;
graphene_rect_t start_rect;
graphene_rect_t end_rect;
float opacity;
float progress;
#endif /* GSK_PREAMBLE */

#include "gskgpucrossfadeinstance.glsl"

PASS(0) vec2 _pos;
PASS(1) vec2 _start_coord;
PASS(2) vec2 _end_coord;
PASS_FLAT(3) float _start_opacity;
PASS_FLAT(4) float _end_opacity;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect b = rect_from_gsk (in_bounds);
  
  pos = rect_get_position (b);

  _pos = pos;

  Rect start_rect = rect_from_gsk (in_start_rect);
  _start_coord = rect_get_coord (start_rect, pos);
  _start_opacity = in_opacity_progress[0] * (1.0 - in_opacity_progress[1]);

  Rect end_rect = rect_from_gsk (in_end_rect);
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
                                   _start_opacity);
  vec4 end = output_color_alpha (texture (GSK_TEXTURE1, _end_coord),
                                 _end_opacity);

  color = start + end;
  position = _pos;
}

#endif

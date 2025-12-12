#define GSK_N_TEXTURES 2

#include "common.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS_FLAT(2) Rect _child_rect;
PASS(3) vec2 _displacement_coord;
PASS_FLAT(4) uvec2 _channels;
PASS_FLAT(5) vec2 _max;
PASS_FLAT(6) vec2 _scale;
PASS_FLAT(7) vec2 _offset;
PASS_FLAT(8) float _opacity;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_displacement_rect;
IN(2) vec4 in_child_rect;
IN(3) uvec2 in_channels;
IN(4) vec2 in_max;
IN(5) vec2 in_scale;
IN(6) vec2 in_offset;
IN(7) float in_opacity;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;

  Rect displacement_rect = rect_from_gsk (in_displacement_rect);
  _displacement_coord = rect_get_coord (displacement_rect, pos);

  Rect child_rect = rect_from_gsk (in_child_rect);
  _child_rect = child_rect;

  _channels = in_channels;
  _max = GSK_GLOBAL_SCALE * in_max;
  _scale = GSK_GLOBAL_SCALE * in_scale;
  _offset = in_offset;
  _opacity = in_opacity;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec4 displace_sample = texture (GSK_TEXTURE0, _displacement_coord);
  vec2 displace = vec2 (displace_sample[_channels[0]], displace_sample[_channels[1]]);
  displace = _scale * (displace - _offset);
  displace = clamp (displace, -_max, _max);
  vec2 child_coord = rect_get_coord (_child_rect, _pos + displace);

  color = output_color_alpha (texture (GSK_TEXTURE1, child_coord),
                              rect_coverage (_rect, _pos) *
                              _opacity);
  position = _pos;
}

#endif

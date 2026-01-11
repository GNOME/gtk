#ifdef GSK_PREAMBLE
textures = 1;
var_name = "gsk_gpu_color_matrix";
struct_name = "GskGpuColorMatrix";
opacity = false;

graphene_matrix_t color_matrix;
graphene_vec4_t color_offset;
graphene_rect_t rect;
graphene_rect_t tex_rect;
#endif

#include "gskgpucolormatrixinstance.glsl"

PASS_FLAT(0) mat4 _color_matrix;
PASS_FLAT(4) vec4 _color_offset;
PASS(5) vec2 _pos;
PASS_FLAT(6) Rect _rect;
PASS(7) vec2 _tex_coord;


#ifdef GSK_VERTEX_SHADER

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


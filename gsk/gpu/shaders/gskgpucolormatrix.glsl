#ifdef GSK_PREAMBLE
textures = 1;
var_name = "gsk_gpu_color_matrix";
struct_name = "GskGpuColorMatrix";

graphene_matrix_t color_matrix;
graphene_vec4_t color_offset;
graphene_rect_t bounds;
graphene_rect_t tex_rect;
float opacity;
#endif /* GSK_PREAMBLE */

#include "gskgpucolormatrixinstance.glsl"

PASS_FLAT(0) mat4 _color_matrix;
PASS_FLAT(4) vec4 _color_offset;
PASS(5) vec2 _pos;
PASS_FLAT(6) Rect _bounds;
PASS(7) vec2 _tex_coord;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect b = rect_from_gsk (in_bounds);
  
  pos = rect_get_position (b);

  _pos = pos;
  _bounds = b;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
  mat4 cm = in_color_matrix;
  if (in_opacity < 1.0)
    {
      cm *= mat4(1.0, 0.0, 0.0, 0.0,
                 0.0, 1.0, 0.0, 0.0,
                 0.0, 0.0, 1.0, 0.0,
                 0.0, 0.0, 0.0, in_opacity);
    }
    
  _color_matrix = cm;
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

  color = output_color_alpha (pixel, rect_coverage (_bounds, _pos));
  position = _pos;
}

#endif


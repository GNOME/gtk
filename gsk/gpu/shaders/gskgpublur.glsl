#ifdef GSK_PREAMBLE
textures = 1;

graphene_rect_t rect;
GdkColor blur_color;
graphene_rect_t tex_rect;
graphene_size_t blur_direction;
float opacity;

variation: gboolean colorize;
#endif /* GSK_PREAMBLE */

#include "gskgpublurinstance.glsl"

/* blur radius (aka in_blur_direction) 0 is NOT supported and MUST be caught before */

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS_FLAT(2) vec4 _blur_color;
PASS(3) vec2 _tex_coord;
PASS_FLAT(4) vec2 _tex_blur_step;
PASS_FLAT(5) uint _samples_per_side;
PASS_FLAT(6) float _sigma;
PASS_FLAT(8) float _opacity;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _blur_color = output_color_from_alt (in_blur_color);
  Rect tex_rect = rect_from_gsk (in_tex_rect);
  _tex_coord = rect_get_coord (tex_rect, pos);

  float sigma = length (GSK_GLOBAL_SCALE * in_blur_direction);
  _tex_blur_step = GSK_GLOBAL_SCALE * in_blur_direction / sigma / rect_size (tex_rect);
  _samples_per_side = uint (3.0 * ceil (sigma));
  _sigma = sigma;
  _opacity = in_opacity;
}

#endif


#ifdef GSK_FRAGMENT_SHADER

// Partially from http://callumhay.blogspot.com/2010/09/gaussian-blur-shader-glsl.html
void
run (out vec4 color,
     out vec2 position)
{
  float factor = -0.5 / (_sigma * _sigma);
  vec4 sum = texture (GSK_TEXTURE0, _tex_coord);
  float coefficient_sum = 1.0;
  vec2 p = _tex_blur_step;

  for (uint i = 0u; i < _samples_per_side; i++)
    {
      float weight = exp (factor * float (i + 1u) * float (i + 1u));
      sum += texture (GSK_TEXTURE0, _tex_coord - p) * weight;
      sum += texture (GSK_TEXTURE0, _tex_coord + p) * weight;

      coefficient_sum += 2.0 * weight;

      p += _tex_blur_step;
    }

  if (VARIATION_COLORIZE)
    color = output_color_alpha (_blur_color, _opacity * sum.a / coefficient_sum);
  else
    color = output_color_alpha (sum / coefficient_sum, _opacity);
  position = _pos;
}

#endif

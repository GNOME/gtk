#define GSK_N_TEXTURES 1

#define GSK_COMPONENT_TRANSFER_IDENTITY 0u
#define GSK_COMPONENT_TRANSFER_LEVELS 1u
#define GSK_COMPONENT_TRANSFER_LINEAR 2u
#define GSK_COMPONENT_TRANSFER_GAMMA 3u
#define GSK_COMPONENT_TRANSFER_DISCRETE 4u
#define GSK_COMPONENT_TRANSFER_TABLE 5u

#include "common.glsl"
#include "color.glsl"

PASS_FLAT(0) vec4 _params_r;
PASS_FLAT(1) vec4 _params_g;
PASS_FLAT(2) vec4 _params_b;
PASS_FLAT(3) vec4 _params_a;
PASS_FLAT(4) vec4 _table0;
PASS_FLAT(5) vec4 _table1;
PASS_FLAT(6) vec4 _table2;
PASS_FLAT(7) vec4 _table3;
PASS_FLAT(8) vec4 _table4;
PASS_FLAT(9) vec4 _table5;
PASS_FLAT(10) vec4 _table6;
PASS_FLAT(11) vec4 _table7;
PASS(12) vec2 _pos;
PASS_FLAT(13) Rect _rect;
PASS(14) vec2 _tex_coord;
PASS_FLAT(15) float _opacity;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_params_r;
IN(1) vec4 in_params_g;
IN(2) vec4 in_params_b;
IN(3) vec4 in_params_a;
IN(4) vec4 in_table0;
IN(5) vec4 in_table1;
IN(6) vec4 in_table2;
IN(7) vec4 in_table3;
IN(8) vec4 in_table4;
IN(9) vec4 in_table5;
IN(10) vec4 in_table6;
IN(11) vec4 in_table7;
IN(12) vec4 in_rect;
IN(13) vec4 in_tex_rect;
IN(14) float in_opacity;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);

  pos = rect_get_position (r);

  _pos = pos;
  _opacity = in_opacity;
  _rect = r;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
  _params_r = in_params_r;
  _params_g = in_params_g;
  _params_b = in_params_b;
  _params_a = in_params_a;
  _table0 = in_table0;
  _table1 = in_table1;
  _table2 = in_table2;
  _table3 = in_table3;
  _table4 = in_table4;
  _table5 = in_table5;
  _table6 = in_table6;
  _table7 = in_table7;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

float
get_table_value (uint k)
{
  if (k < 4u)
    return _table0[k];
  else if (k < 8u)
    return _table1[k-4u];
  else if (k < 12u)
    return _table2[k-8u];
  else if (k < 16u)
    return _table3[k-12u];
  else if (k < 20u)
    return _table4[k-16u];
  else if (k < 24u)
    return _table5[k-20u];
  else if (k < 28u)
    return _table6[k-24u];
  else if (k < 32u)
    return _table7[k-28u];
  return 0.;
}

float
get_param(uint coord, uint k)
{
  if (coord == 0u)
    return _params_r[k];
  else if (coord == 1u)
    return _params_g[k];
  else if (coord == 2u)
    return _params_b[k];
  else
    return _params_a[k];
}

float
apply_component_transfer (uint coord, float c)
{
  uint kind = uint(get_param(coord, 0u));

  if (kind == GSK_COMPONENT_TRANSFER_IDENTITY)
    {
      return c;
    }
  else if (kind == GSK_COMPONENT_TRANSFER_LEVELS)
    {
      float n = get_param(coord, 1u);
      return (floor (c * n) + 0.5) / n;
    }
  else if (kind == GSK_COMPONENT_TRANSFER_LINEAR)
    {
      float m = get_param(coord, 1u);
      float b = get_param(coord, 2u);
      return c * m + b;
    }
  else if (kind == GSK_COMPONENT_TRANSFER_GAMMA)
    {
      float a = get_param(coord, 1u);
      float e = get_param(coord, 2u);
      float o = get_param(coord, 3u);
      return a * pow(c, e) + o;
    }
  else if (kind == GSK_COMPONENT_TRANSFER_DISCRETE)
    {
      float n = get_param(coord, 1u);
      uint base = uint(get_param(coord, 2u));
      uint k;
      for (k = 0u; float(k) < n; k++)
        {
          if (float(k) <= c*n && c*n < float(k+1u))
            return get_table_value (base + k);
        }
      return get_table_value (base + k - 1u);
    }
  else if (kind == GSK_COMPONENT_TRANSFER_TABLE)
    {
      float n = get_param(coord, 1u) - 1.;
      uint base = uint(get_param(coord, 2u));
      uint k;
      for (k = 0u; float(k) < n; k++)
        {
          if (float(k) <= c*n && c*n < float(k+1u))
            {
              float vk = get_table_value (base + k);
              float vk2 = get_table_value (base + k + 1u);
              return vk + (c - float(k) / n) * n * (vk2 - vk);
            }
        }
      return get_table_value (base + k);
    }
}

void
run (out vec4 color,
     out vec2 position)
{
  vec4 pixel = texture (GSK_TEXTURE0, _tex_coord);
  pixel = alt_color_from_output (pixel);

  pixel = color_unpremultiply (pixel);

  pixel.r = apply_component_transfer (0u, pixel.r);
  pixel.g = apply_component_transfer (1u, pixel.g);
  pixel.b = apply_component_transfer (2u, pixel.b);
  pixel.a = apply_component_transfer (3u, pixel.a);

  pixel = clamp (pixel, 0.0, 1.0);

  pixel = color_premultiply (pixel);

  pixel = output_color_from_alt (pixel);

  color = output_color_alpha (pixel, rect_coverage (_rect, _pos) * _opacity);
  position = _pos;
}

#endif


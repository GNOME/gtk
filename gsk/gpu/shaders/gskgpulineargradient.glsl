#ifdef GSK_PREAMBLE
var_name = "gsk_gpu_linear_gradient";
struct_name = "GskGpuLinearGradient";
acs_premultiplied = true;

graphene_rect_t rect;
graphene_point_t start;
graphene_point_t end;
GdkColor color0;
GdkColor color1;
GdkColor color2;
GdkColor color3;
GdkColor color4;
GdkColor color5;
GdkColor color6;
graphene_vec4_t offsets0;
graphene_vec4_t offsets1;
graphene_vec4_t hints0;
graphene_vec4_t hints1;

variation: gboolean supersampling;
variation: gboolean premultiplied;
variation: GskRepeat repeat;
#endif /* GSK_PREAMBLE */

#include "gskgpulineargradientinstance.glsl"

#include "enums.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS_FLAT(2) vec4 _color0;
PASS_FLAT(3) vec4 _color1;
PASS_FLAT(4) vec4 _color2;
PASS_FLAT(5) vec4 _color3;
PASS_FLAT(6) vec4 _color4;
PASS_FLAT(7) vec4 _color5;
PASS_FLAT(8) vec4 _color6;
PASS_FLAT(9) vec4 _offsets0;
PASS_FLAT(10) vec4 _offsets1;
PASS_FLAT(11) vec4 _hints0;
PASS_FLAT(12) vec4 _hints1;
PASS(13) float _offset;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  vec2 start = in_start;
  vec2 end = in_end;
  vec2 line = end - start;
  float line_length = dot (line, line);
  _offset = dot (pos / GSK_GLOBAL_SCALE - start, line) / line_length;

  _pos = pos;
  _rect = r;
  if (VARIATION_PREMULTIPLIED)
    {
      _color0 = color_premultiply (in_color0);
      _color1 = color_premultiply (in_color1);
      _color2 = color_premultiply (in_color2);
      _color3 = color_premultiply (in_color3);
      _color4 = color_premultiply (in_color4);
      _color5 = color_premultiply (in_color5);
      _color6 = color_premultiply (in_color6);
    }
  else
    {
      _color0 = in_color0;
      _color1 = in_color1;
      _color2 = in_color2;
      _color3 = in_color3;
      _color4 = in_color4;
      _color5 = in_color5;
      _color6 = in_color6;
    }
  _offsets0 = in_offsets0;
  _offsets1 = in_offsets1;
  _hints0 = in_hints0;
  _hints1 = in_hints1;
}

#endif


#ifdef GSK_FRAGMENT_SHADER

#define M_LN2 0.69314718055994530942 /* log_e 2 */

float
compute_c (float f, float hint)
{
  if (hint == 0.5)
    return f;
  else if (hint <= 0.0)
    return 1.0;
  else if (hint >= 1.0)
    return 0.0;
  else
    return pow(f, -M_LN2 / log(hint));
}

vec4
maybe_premultiply (vec4 color)
{
  if (VARIATION_PREMULTIPLIED)
    return color;
  else
    return color_premultiply (color);
}

vec4
get_gradient_color (float offset)
{
  vec4 color;
  float f;

  if (offset <= _offsets0[3])
    {
      if (offset <= _offsets0[1])
        {
          if (offset <= _offsets0[0])
            color = _color0;
          else
            {
              f = (offset - _offsets0[0]) / (_offsets0[1] - _offsets0[0]);
              f = compute_c (f, _hints0[1]);
              color = mix (_color0, _color1, f);
            }
        }
      else
        {
          if (offset <= _offsets0[2])
            {
              f = (offset - _offsets0[1]) / (_offsets0[2] - _offsets0[1]);
              f = compute_c (f, _hints0[2]);
              color = mix (_color1, _color2, f);
            }
          else
            {
              f = (offset - _offsets0[2]) / (_offsets0[3] - _offsets0[2]);
              f = compute_c (f, _hints0[3]);
              color = mix (_color2, _color3, f);
            }
        }
    }
  else
    {
      if (offset <= _offsets1[1])
        {
          if (offset <= _offsets1[0])
            {
              f = (offset - _offsets0[3]) / (_offsets1[0] - _offsets0[3]);
              f = compute_c (f, _hints1[0]);
              color = mix (_color3, _color4, f);
            }
          else
            {
              f = (offset - _offsets1[0]) / (_offsets1[1] - _offsets1[0]);
              f = compute_c (f, _hints1[1]);
              color = mix (_color4, _color5, f);
            }
        }
      else
        {
          if (offset <= _offsets1[2])
            {
              f = (offset - _offsets1[1]) / (_offsets1[2] - _offsets1[1]);
              f = compute_c (f, _hints1[2]);
              color = mix (_color5, _color6, f);
            }
          else
            color = _color6;
        }
    }

  return maybe_premultiply (color);
}

vec4
get_gradient_color_at (float offset)
{
  switch (VARIATION_REPEAT)
    {
    case GSK_REPEAT_NONE:
      if (offset < 0.0 || offset > 1.0)
        return vec4(0.0, 0.0, 0.0, 0.0);
      break;
    case GSK_REPEAT_REPEAT:
      offset = fract (offset);
      break;
    case GSK_REPEAT_REFLECT:
      if ((int (floor (offset))) % 2 == 0)
        offset = fract (offset);
      else
        offset = 1.0 - fract (offset);
      break;
    case GSK_REPEAT_PAD:
      if (offset <= 0.0)
        return maybe_premultiply (_color0);
      else if (offset >= 1.0)
        return maybe_premultiply (_color6);
      break;
    default:
      break;
    }

  return output_color_from_alt (get_gradient_color (offset));
}

void
run (out vec4 color,
     out vec2 position)
{
  float alpha = rect_coverage (_rect, _pos);

  if (VARIATION_SUPERSAMPLING)
    {
      float dx = 0.25 * dFdx (_offset);
      float dy = 0.25 * dFdy (_offset);
      color = output_color_alpha (get_gradient_color_at (_offset - dx - dy) +
                                  get_gradient_color_at (_offset - dx + dy) +
                                  get_gradient_color_at (_offset + dx - dy) +
                                  get_gradient_color_at (_offset + dx + dy),
                                  alpha * 0.25);
    }
  else
    {
      color = output_color_alpha (get_gradient_color_at (_offset), alpha);
    }

  position = _pos;
}

#endif

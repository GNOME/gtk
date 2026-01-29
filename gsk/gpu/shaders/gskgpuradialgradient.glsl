#ifdef GSK_PREAMBLE
var_name = "gsk_gpu_radial_gradient";
struct_name = "GskGpuRadialGradient";
acs_premultiplied = true;

graphene_rect_t rect;
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
graphene_point_t start_center;
graphene_size_t start_radius;
graphene_point_t end_center;
graphene_size_t end_radius;

variation: gboolean supersampling;
variation: gboolean concentric;
variation: gboolean premultiplied;
variation: GskRepeat repeat;
#endif /* GSK_PREAMBLE */

#include "gskgpuradialgradientinstance.glsl"

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
PASS_FLAT(13) vec4 _start_circle; /* xy are center, zw the radii */
PASS_FLAT(14) vec4 _end_circle;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);

  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;

  _start_circle = vec4 (in_start_center, in_start_radius);
  _end_circle = vec4 (in_end_center, in_end_radius);

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
get_gradient_color (float offset)
{
  vec4 color;
  float f;

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
        return _color0;
      else if (offset >= 1.0)
        return _color6;
      break;
    default:
      return vec4(1.0, 0.0, 0.8, 1.0);
    }

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

  if (VARIATION_PREMULTIPLIED)
    return color;
  else
    return color_premultiply (color);
}

vec4
get_gradient_color_at (vec2 pos)
{
  vec2 scale = vec2 (1, _end_circle.z / _end_circle.w);

  if (VARIATION_CONCENTRIC)
    {
      float off = length ((pos - _end_circle.xy) * scale);
      off = (off - _start_circle.z) / (_end_circle.z - _start_circle.z);
      return output_color_from_alt (get_gradient_color (off));
    }
  else
    {
      float off;

      vec2 c1 = _start_circle.xy * scale;
      float r1 = _start_circle.z;
      vec2 c2 = c1 + (_end_circle.xy - _start_circle.xy) * scale;
      float r2 = _end_circle.z;

      vec2 p = c1 + (pos - _start_circle.xy) * scale;

      vec2 cd = c2 - c1;
      vec2 pd = p - c1;
      float dr = r2 - r1;

      float a = cd.x * cd.x + cd.y * cd.y - dr * dr;
      float b = pd.x * cd.x + pd.y * cd.y + r1 * dr;
      float c = pd.x * pd.x + pd.y * pd.y - r1 * r1;

      if (a == 0.0)
        {
          if (b != 0.0)
            {
              float t = 1.0/2.0 * c / b;
              if (t * dr >= -r1)
                return output_color_from_alt (get_gradient_color (t));
            }

          return output_color_from_alt (vec4(0.0, 0.0, 0.0, 0.0));
        }

      float discr = b * b - a * c;

      if (discr >= 0.0)
        {
          float sqrtdiscr = sqrt (discr);
          float t0 = (b + sqrtdiscr) / a;
          float t1 = (b - sqrtdiscr) / a;

          if (t0 * dr >= -r1)
            return output_color_from_alt (get_gradient_color (t0));
          else if (t1 * dr >= -r1)
            return output_color_from_alt (get_gradient_color (t1));
        }
    }

  return output_color_from_alt (vec4(0.0, 0.0, 0.0, 0.0));
}

void
run (out vec4 color,
     out vec2 position)
{
  float alpha = rect_coverage (_rect, _pos);

  vec2 pos = _pos / GSK_GLOBAL_SCALE;
  if (VARIATION_SUPERSAMPLING)
    {
      vec2 dpos = 0.25 * fwidth (pos);
      color = output_color_alpha (get_gradient_color_at (pos + vec2(- dpos.x, - dpos.y)) +
                                  get_gradient_color_at (pos + vec2(- dpos.x,   dpos.y)) +
                                  get_gradient_color_at (pos + vec2(  dpos.x, - dpos.y)) +
                                  get_gradient_color_at (pos + vec2(  dpos.x,   dpos.y)),
                                  0.25 * alpha);
    }
  else
    {
      color = output_color_alpha (get_gradient_color_at (pos), alpha);
    }

  position = _pos;
}

#endif

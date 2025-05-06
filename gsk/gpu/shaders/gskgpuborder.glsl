#define GSK_N_TEXTURES 0

#include "common.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) vec4 _color;
PASS_FLAT(2) RoundedRect _outside;
PASS_FLAT(5) RoundedRect _inside;



#ifdef GSK_VERTEX_SHADER

IN(0) mat4 in_border_colors;
IN(4) mat3x4 in_outline;
IN(7) vec4 in_border_widths;
IN(8) vec2 in_offset;

vec4
compute_color (void)
{
  uint triangle_index = uint (GSK_VERTEX_INDEX) / 3u;

  switch (triangle_index)
  {
    case 2u * SLICE_TOP_LEFT + 1u:
      if (in_border_widths[TOP] > 0.0)
        return output_color_from_alt (in_border_colors[TOP]);
      else
        return output_color_from_alt (in_border_colors[LEFT]);

    case 2u * SLICE_TOP:
    case 2u * SLICE_TOP + 1u:
      return output_color_from_alt (in_border_colors[TOP]);

    case 2u * SLICE_TOP_RIGHT:
      if (in_border_widths[TOP] > 0.0)
        return output_color_from_alt (in_border_colors[TOP]);
      else
        return output_color_from_alt (in_border_colors[RIGHT]);

    case 2u * SLICE_TOP_RIGHT + 1u:
      if (in_border_widths[RIGHT] > 0.0)
        return output_color_from_alt (in_border_colors[RIGHT]);
      else
        return output_color_from_alt (in_border_colors[TOP]);

    case 2u * SLICE_RIGHT:
    case 2u * SLICE_RIGHT + 1u:
      return output_color_from_alt (in_border_colors[RIGHT]);

    case 2u * SLICE_BOTTOM_RIGHT:
      if (in_border_widths[RIGHT] > 0.0)
        return output_color_from_alt (in_border_colors[RIGHT]);
      else
        return output_color_from_alt (in_border_colors[BOTTOM]);

    case 2u * SLICE_BOTTOM_RIGHT + 1u:
      if (in_border_widths[BOTTOM] > 0.0)
        return output_color_from_alt (in_border_colors[BOTTOM]);
      else
        return output_color_from_alt (in_border_colors[RIGHT]);

    case 2u * SLICE_BOTTOM:
    case 2u * SLICE_BOTTOM + 1u:
      return output_color_from_alt (in_border_colors[BOTTOM]);

    case 2u * SLICE_BOTTOM_LEFT:
      if (in_border_widths[BOTTOM] > 0.0)
        return output_color_from_alt (in_border_colors[BOTTOM]);
      else
        return output_color_from_alt (in_border_colors[LEFT]);

    case 2u * SLICE_BOTTOM_LEFT + 1u:
      if (in_border_widths[LEFT] > 0.0)
        return output_color_from_alt (in_border_colors[LEFT]);
      else
        return output_color_from_alt (in_border_colors[BOTTOM]);

    case 2u * SLICE_LEFT:
    case 2u * SLICE_LEFT + 1u:
      return output_color_from_alt (in_border_colors[LEFT]);

    case 2u * SLICE_TOP_LEFT:
      if (in_border_widths[LEFT] > 0.0)
        return output_color_from_alt (in_border_colors[LEFT]);
      else
        return output_color_from_alt (in_border_colors[TOP]);

    default:
      return output_color_from_alt (in_border_colors[TOP]);
  }
}

void
run (out vec2 pos)
{
  vec4 border_widths = in_border_widths * GSK_GLOBAL_SCALE.yxyx;
  RoundedRect outside = rounded_rect_from_gsk (in_outline);
  RoundedRect inside = rounded_rect_shrink (outside, border_widths);
  rounded_rect_offset (inside, in_offset * GSK_GLOBAL_SCALE);

  pos = border_get_position (outside, inside);

  _pos = pos;
  _color = compute_color ();
  _outside = outside;
  _inside = inside;
}

#endif

#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  float alpha = clamp (rounded_rect_coverage (_outside, _pos) -
                       rounded_rect_coverage (_inside, _pos),
                       0.0, 1.0);

  position = _pos;
  color = output_color_alpha (_color, alpha);
}

#endif

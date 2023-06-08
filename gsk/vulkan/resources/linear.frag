#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in flat Rect inRect;
layout(location = 2) in float inGradientPos;
layout(location = 3) in flat int inRepeating;
layout(location = 4) in flat int inStopOffset;
layout(location = 5) in flat int inStopCount;

layout(location = 0) out vec4 color;

float
get_offset (int i)
{
  return get_float (inStopOffset + i * 5);
}

vec4
get_color (int i)
{
  i = clamp (i, 0, inStopCount - 1);
  return color = vec4 (get_float (inStopOffset + i * 5 + 1),
                       get_float (inStopOffset + i * 5 + 2),
                       get_float (inStopOffset + i * 5 + 3),
                       get_float (inStopOffset + i * 5 + 4));
}

vec4
get_color_for_range_unscaled (float start,
                             float end)
{
  vec4 result = vec4 (0);
  float offset;
  int i;

  for (i = 0; i < inStopCount; i++)
    {
      offset = get_offset (i);
      if (offset >= start)
        break;
    }
  if (i == inStopCount)
    offset = 1;

  float last_offset = i > 0 ? get_offset (i - 1) : 0;
  vec4 last_color = get_color (i - 1);
  vec4 color = get_color (i);
  if (last_offset < start)
    {
      last_color = mix (last_color, color, (start - last_offset) / (offset - last_offset));
      last_offset = start;
    }
  if (end <= start)
    return last_color;

  for (; i < inStopCount; i++)
    {
      offset = get_offset (i);
      color = get_color (i);
      if (offset >= end)
        break;
      result += 0.5 * (color + last_color) * (offset - last_offset);
      last_offset = offset;
      last_color = color;
    }
  if (i == inStopCount)
    {
      offset = 1;
      color = get_color (i);
    }
  if (offset > end)
    {
      color = mix (last_color, color, (end - last_offset) / (offset - last_offset));
      offset = end;
    }
  result += 0.5 * (color + last_color) * (offset - last_offset);

  return result;
}

vec4
get_color_for_range (float start,
                     float end)
{
  return get_color_for_range_unscaled (start, end) / (end - start);
}

void main()
{
  vec4 c;
  float pos_start, pos_end;
  float dPos = 0.5 * abs (fwidth (inGradientPos));

  if (inRepeating != 0)
    {
      pos_start = inGradientPos - dPos;
      pos_end = inGradientPos + dPos;
      if (floor (pos_end) > floor (pos_start))
        {
          float fract_end = fract(pos_end);
          float fract_start = fract(pos_start);
          float n = floor (pos_end) - floor (pos_start);
          if (fract_end > fract_start + 0.01)
            c = get_color_for_range_unscaled (fract_start, fract_end);
          else if (fract_start > fract_end + 0.01)
            c = -get_color_for_range_unscaled (fract_end, fract_start);
          c += get_color_for_range_unscaled (0.0, 1.0) * n;
          c /= pos_end - pos_start;
        }
      else
        {
          c = get_color_for_range (fract (pos_start), fract (pos_end));
        }
    }
  else
    {
      pos_start = clamp (inGradientPos - dPos, 0, 1);
      pos_end = clamp (inGradientPos + dPos, 0, 1);
      c = get_color_for_range (pos_start, pos_end);
    }

  float alpha = c.a * rect_coverage (inRect, inPos);
  color = clip_scaled (inPos, vec4(c.rgb, 1) * alpha);
}

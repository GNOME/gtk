#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

struct ColorStop {
  float offset;
  vec4 color;
};

layout(location = 0) in vec2 inPos;
layout(location = 1) in flat Rect inRect;
layout(location = 2) in float inGradientPos;
layout(location = 3) in flat int inRepeating;
layout(location = 4) in flat int inStopOffset;
layout(location = 5) in flat int inStopCount;

layout(location = 0) out vec4 color;

ColorStop
get_stop(int i)
{
  ColorStop result;

  result.offset = get_float(inStopOffset + i * 5);
  result.color = vec4(get_float(inStopOffset + i * 5 + 1),
                      get_float(inStopOffset + i * 5 + 2),
                      get_float(inStopOffset + i * 5 + 3),
                      get_float(inStopOffset + i * 5 + 4));

  return result;
}

void main()
{
  float pos;

  if (inRepeating != 0)
    pos = fract (inGradientPos);
  else
    pos = clamp (inGradientPos, 0, 1);

  ColorStop stop = get_stop (0);
  float last_offset = stop.offset;
  color = stop.color;
  for (int i = 1; i < inStopCount; i++)
    {
      stop = get_stop(i);
      if (stop.offset < pos)
        color = stop.color;
      else
        color = mix (color, stop.color, clamp((pos - last_offset) / (stop.offset - last_offset), 0, 1));
      last_offset = stop.offset;
      if (last_offset >= pos)
        break;
    }
  
  if (last_offset < pos)
    color = mix (color, stop.color, clamp((pos - last_offset) / (1 - last_offset), 0, 1));

  float alpha = color.a * rect_coverage (inRect, inPos);
  color = clip_scaled (inPos, vec4(color.rgb, 1) * alpha);
}

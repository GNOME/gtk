#version 420 core

#include "clip.frag.glsl"

struct ColorStop {
  float offset;
  vec4 color;
};

layout(location = 0) in vec2 inPos;
layout(location = 1) in float inGradientPos;
layout(location = 2) in flat int inRepeating;
layout(location = 3) in flat int inStopCount;
layout(location = 4) in flat ColorStop inStops[8];

layout(location = 0) out vec4 outColor;

void main()
{
  float pos;
  if (inRepeating != 0)
    pos = fract (inGradientPos);
  else
    pos = clamp (inGradientPos, 0, 1);

  vec4 color = inStops[0].color;
  int n = clamp (inStopCount, 2, 8);
  for (int i = 1; i < n; i++)
    {
      if (inStops[i].offset > inStops[i-1].offset)
        color = mix (color, inStops[i].color, clamp((pos - inStops[i-1].offset) / (inStops[i].offset - inStops[i-1].offset), 0, 1));
    }
  
  //outColor = vec4(pos, pos, pos, 1.0);
  outColor = clip (inPos, color);
}

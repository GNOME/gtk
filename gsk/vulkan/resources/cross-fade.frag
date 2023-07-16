#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in Rect inStartRect;
layout(location = 2) in Rect inEndRect;
layout(location = 3) in vec2 inStartTexCoord;
layout(location = 4) in vec2 inEndTexCoord;
layout(location = 5) flat in uint inStartTexId;
layout(location = 6) flat in uint inEndTexId;
layout(location = 7) in float inProgress;

layout(location = 0) out vec4 color;

void main()
{
  float start_alpha = rect_coverage (inStartRect, inPos);
  vec4 start = texture (get_sampler (inStartTexId), inStartTexCoord) * start_alpha;
  float end_alpha = rect_coverage (inEndRect, inPos);
  vec4 end = texture (get_sampler (inEndTexId), inEndTexCoord) * end_alpha;

  color = clip_scaled (inPos, mix (start, end, inProgress));
}

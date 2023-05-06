#ifndef _RECT_FRAG_
#define _RECT_FRAG_

#include "rect.glsl"

float
rect_coverage (Rect r, vec2 p)
{
  vec2 dFdp = abs(fwidth (p));
  Rect prect = Rect(vec4(p - 0.5 * dFdp, p + 0.5 * dFdp));
  Rect coverect = rect_intersect (r, prect);
  vec2 coverage = rect_size(coverect) / dFdp;
  return coverage.x * coverage.y;
}

#endif

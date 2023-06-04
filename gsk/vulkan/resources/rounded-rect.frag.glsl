#ifndef _ROUNDED_RECT_FRAG_
#define _ROUNDED_RECT_FRAG_

#include "rounded-rect.glsl"

float
rounded_rect_coverage (RoundedRect r, vec2 p)
{
  vec2 fw = abs (fwidth (p));
  float distance_scale = max (fw.x, fw.y);
  float distance = rounded_rect_distance (r, p) / distance_scale;
  float coverage = 0.5 - distance;

  return clamp (coverage, 0, 1);
}

#endif

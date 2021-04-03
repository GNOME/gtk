#include "constants.glsl"

#ifndef _CLIP_
#define _CLIP_

vec4 intersect(vec4 a, vec4 b)
{
  a = vec4(a.xy, a.xy + a.zw);
  b = vec4(b.xy, b.xy + b.zw);
  vec4 result = vec4(max(a.xy, b.xy), min(a.zw, b.zw));
  if (any (greaterThanEqual (result.xy, result.zw)))
    return vec4(0.0,0.0,0.0,0.0);
  return vec4(result.xy, result.zw - result.xy);
}

#ifdef CLIP_ROUNDED_RECT
vec4 clip(vec4 rect)
{
  /* rounded corner clipping is done in fragment shader */
  return intersect(rect, push.clip_bounds);
}
#elif defined(CLIP_RECT)
vec4 clip(vec4 rect)
{
  return intersect(rect, push.clip_bounds);
}
#elif defined(CLIP_NONE)
vec4 clip(vec4 rect)
{
  return rect;
}
#else
#error "No clipping define given. Need CLIP_NONE, CLIP_RECT or CLIP_ROUNDED_RECT"
#endif

#endif

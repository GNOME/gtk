#include "constants.glsl"
#include "rounded-rect.frag.glsl"

#ifndef _CLIP_
#define _CLIP_

#ifdef CLIP_ROUNDED_RECT

vec4
clip_scaled (vec2 pos, vec4 color)
{
  RoundedRect r = RoundedRect(vec4(push.clip_bounds.xy, push.clip_bounds.xy + push.clip_bounds.zw), push.clip_widths, push.clip_heights);

  r = rounded_rect_scale (r, push.scale);

  return color * rounded_rect_coverage (r, pos);
}

#elif defined(CLIP_RECT)

vec4
clip_scaled (vec2 pos, vec4 color)
{
  return color;
}

#elif defined(CLIP_NONE)

vec4
clip_scaled (vec2 pos, vec4 color)
{
  return color;
}

#else
#error "No clipping define given. Need CLIP_NONE, CLIP_RECT or CLIP_ROUNDED_RECT"
#endif

vec4
clip (vec2 pos, vec4 color)
{
  return clip_scaled (pos * push.scale, color);
}

#endif

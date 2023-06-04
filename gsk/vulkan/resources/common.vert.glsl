#ifndef _COMMON_VERT_
#define _COMMON_VERT_

#include "clip.vert.glsl"
#include "constants.glsl"
#include "rect.glsl"

vec2 offsets[6] = { vec2(0.0, 0.0),
                    vec2(1.0, 0.0),
                    vec2(0.0, 1.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 0.0),
                    vec2(1.0, 1.0) };

vec2
set_position_from_rect (Rect rect)
{
  Rect r = rect_round_larger (clip_rect (rect));

  vec2 pos = mix (r.bounds.xy, r.bounds.zw, offsets[gl_VertexIndex]);
  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);

  return pos;
}

vec2
scale_tex_coord (vec2 in_pos,
                 Rect in_rect,
                 vec4 tex_rect)
{
  return tex_rect.xy + (in_pos - in_rect.bounds.xy) / rect_size (in_rect) * tex_rect.zw;
}

#endif

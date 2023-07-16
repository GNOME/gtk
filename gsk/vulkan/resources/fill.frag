#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

#define GSK_FILL_RULE_WINDING 0
#define GSK_FILL_RULE_EVEN_ODD 1

layout(location = 0) in vec2 in_pos;
layout(location = 1) in flat Rect in_rect;
layout(location = 2) in flat vec4 in_color;
layout(location = 3) in flat vec2 in_offset;
layout(location = 4) in flat int in_points_id;
layout(location = 5) in flat int in_n_points;
layout(location = 6) in flat int in_fill_rule;

layout(location = 0) out vec4 color;

vec2
get_point (int i)
{
  vec2 p = vec2 (get_float (in_points_id + i * 2),
                 get_float (in_points_id + i * 2 + 1));
  p *= push.scale;

  return p;
}

float
cross2d (vec2 v0, vec2 v1)
{
  return v0.x * v1.y - v0.y * v1.x;
}

float
path_distance (vec2 p)
{
  vec2 start = get_point (0);

  float d = dot (p - start, p - start);
  int w = 0;
  
  for (int i = 1; i <= in_n_points; i++)
    {
      vec2 end = get_point (i % in_n_points);

      // distance
      vec2 e = end - start;
      vec2 v = p - start;
      vec2 pq = v - e * clamp (dot (v, e) / dot(e, e), 0.0, 1.0);
      d = min (d, dot (pq, pq));

      // winding number from http://geomalgorithms.com/a03-_inclusion.html
      vec2 v2 = p - end;
      float val3 = cross2d (e, v); //isLeft
      bvec3 cond = bvec3 (v.y >= 0.0, v2.y < 0.0, val3 > 0.0);
      if (all (cond))
        w++; // have a valid up intersect
      else if (all (not (cond)))
        w--; // have a valid down intersect

      start = end;
    }

  if (in_fill_rule == GSK_FILL_RULE_WINDING)
    {
      if (w == 0)
        return sqrt (d);
      else if (w == 1)
        return - sqrt (d);
      else
        return -1.0; /* not really, but good enough */
    }
  else if (in_fill_rule == GSK_FILL_RULE_EVEN_ODD)
    {
      return sqrt (d) * (w % 2 == 0 ? 1.0 : -1.0);
    }
}

float
path_coverage (vec2 p)
{
  float d = path_distance (p);

  return clamp (0.5 - d, 0.0, 1.0);
}

void main()
{
  float alpha = in_color.a * rect_coverage (in_rect, in_pos);
  alpha *= path_coverage (in_pos - in_offset * push.scale);
  color = clip_scaled (in_pos, vec4(in_color.rgb, 1) * alpha);
}

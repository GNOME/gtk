#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

#define GSK_LINE_CAP_BUTT 0
#define GSK_LINE_CAP_ROUND 1
#define GSK_LINE_CAP_SQUARE 2

#define GSK_LINE_JOIN_MITER 0
#define GSK_LINE_JOIN_MITER_CLIP 1
#define GSK_LINE_JOIN_ROUND 2
#define GSK_LINE_JOIN_BEVEL 3

#define GSK_PATH_MOVE 0
#define GSK_PATH_CLOSE 1
#define GSK_PATH_LINE 2
#define GSK_PATH_CURVE 3
#define GSK_PATH_CONIC 4

layout(location = 0) in vec2 in_pos;
layout(location = 1) in flat Rect in_rect;
layout(location = 2) in flat vec4 in_color;
layout(location = 3) in flat vec2 in_offset;
layout(location = 4) in flat int in_points_id;
layout(location = 5) in flat float in_half_line_width;
layout(location = 6) in flat int in_line_cap;
layout(location = 7) in flat int in_line_join;
layout(location = 8) in flat float in_miter_limit;

layout(location = 0) out vec4 color;

float scale;
float line_width;

vec2
get_point (inout int idx)
{
  vec2 p = vec2 (get_float (idx), get_float (idx + 1));
  idx += 2;

  return p * scale;
}



float
cross2d (vec2 v0, vec2 v1)
{
  return v0.x * v1.y - v0.y * v1.x;
}

float
line_distance (vec2     start,
               vec2     end,
               out vec2 tan)
{
  tan = end - start;
  float offset = dot (-start, tan) / dot (tan, tan);
  if (offset < 0.0 || offset > 1.0)
    return 1e38;
  vec2 pq = start + tan * offset;
  return length (pq) - line_width;
}

float
cubic_bezier_normal_iteration (float t,
                               vec2  a0,
                               vec2  a1,
                               vec2  a2,
                               vec2  a3)
{
  // Horner's method
  vec2 a_2 = a2 + t * a3;
  vec2 a_1 = a1 + t * a_2;
  vec2 b_2 = a_2 + t * a3;

  vec2 uv_to_p = a0 + t * a_1;
  vec2 tang = a_1 + t * b_2;

  float l_tang = dot (tang, tang);
  return t - dot (tang, uv_to_p) / l_tang;
}

void
cubic_distance (vec2        p0,
                vec2        p1,
                vec2        p2,
                vec2        p3,
                inout float d,
                inout vec2  p0_tan,
                inout vec2  p1_tan)
{
  vec2 a3 = (-p0 + 3. * p1 - 3. * p2 + p3);
  vec2 a2 = (3. * p0 - 6. * p1 + 3. * p2);
  vec2 a1 = (-3. * p0 + 3. * p1);
  vec2 a0 = p0;

  p0_tan = a1;
  p1_tan = 3 * a3 + 2 * a2 + a1;

  vec2 tmp = min (min (p0, p1), min (p2, p3)) - line_width;
  if (tmp.x >= d || tmp.y >= d)
    return;
  tmp = max (max (p0, p1), max (p2, p3)) + line_width;
  if (tmp.x <= - d || tmp.y <= - d)
    return;

  float t;
  vec3 params = vec3 (0, .5, 1);

  for (int i = 0; i < 3; i++)
    {
      t = params[i];
      for (int j = 0; j < 3; j++)
        {
          t = cubic_bezier_normal_iteration (t, a0, a1, a2, a3);
        }
      if (t < 0.0 || t > 1.0)
        continue;

      vec2 uv = ((a3 * t + a2) * t + a1) * t + a0;
      d = min (d, length (uv) - line_width);
    }
}

float
cap_distance (vec2 p,
              vec2 p_tan)
{
  switch (in_line_cap)
    {
      case GSK_LINE_CAP_BUTT:
        return 1e38;

      case GSK_LINE_CAP_ROUND:
        return length (p) - line_width;

      case GSK_LINE_CAP_SQUARE:
        vec2 unused;
        return line_distance (p, p - line_width * normalize (p_tan), unused);
    }

  return 0;
}

float
ray_distance (vec2      p,
              vec2      dir,
              out float offset)
{
  offset = dot (-p, dir) / dot (dir, dir);
  vec2 pq = p + dir * offset;
  return length (pq) - line_width;
}

float
join_distance (vec2 p,
               vec2 in_tan,
               vec2 out_tan)
{
  out_tan = - out_tan;

  float in_d, out_d;
  float d = ray_distance (p, in_tan, in_d);
  d = max (d, ray_distance (p, out_tan, out_d));
  if (in_d < 0 || out_d < 0)
    return 1e38;

  if (in_line_join == GSK_LINE_JOIN_ROUND)
    return length (p) - line_width;

  in_tan = normalize (in_tan);
  out_tan = normalize (out_tan);
  vec2 bisector = normalize (in_tan + out_tan);

  float miter_d;
  ray_distance (p, bisector, miter_d);

  float bevel_d;
  ray_distance (line_width * vec2(in_tan.y, -in_tan.x), bisector, bevel_d);
  bevel_d = abs (bevel_d);

  if (in_line_join == GSK_LINE_JOIN_BEVEL)
    return max (d, miter_d - bevel_d);
  else if (in_line_join == GSK_LINE_JOIN_MITER_CLIP)
    return max (d, miter_d - max (bevel_d, in_miter_limit * line_width));

  /* GSK_LINE_JOIN_MITER is the fallback, so no if() */
  if (miter_d > in_miter_limit * line_width)
    return max (d, miter_d - bevel_d);
  else
    return max (d, miter_d - in_miter_limit * line_width);
}

void
standard_contour_stroke_distance (vec2        p,
                                  inout int   path_idx,
                                  inout float d)
{
  int n_points = get_int (path_idx);
  int ops_idx = path_idx + 1;
  int pts_idx = ops_idx + n_points;

  vec2 start = get_point (pts_idx) - p;
  vec2 start_tan = vec2(1, 0);
  vec2 p0 = start;
  vec2 p0_tan;
  int op = GSK_PATH_MOVE;

  for (int i = 1; i < n_points && d > -1.0; i++)
    {
      op = get_int (ops_idx + i);
      switch (op)
        {
          case GSK_PATH_MOVE:
            discard;

          case GSK_PATH_CLOSE:
          case GSK_PATH_LINE:
            {
              vec2 p1 = get_point (pts_idx) - p;
              vec2 line_tan;
              d = min (d, line_distance (p0, p1, line_tan));
              if (i > 1)
                d = min (d, join_distance (p0, p0_tan, line_tan));
              else
                start_tan = line_tan;
              p0_tan = line_tan;
              p0 = p1;
            }
            break;

          case GSK_PATH_CURVE:
            {
              vec2 p1 = get_point (pts_idx) - p;
              vec2 p2 = get_point (pts_idx) - p;
              vec2 p3 = get_point (pts_idx) - p;
              vec2 start, end;
              cubic_distance (p0, p1, p2, p3, d, start, end);
              if (i > 1)
                d = min (d, join_distance (p0, p0_tan, start));
              else
                start_tan = start;
              p0 = p3;
              p0_tan = end;
            }
            break;

          case GSK_PATH_CONIC:
            {
              /* FIXME */
              pts_idx += 4;
              vec2 p1 = get_point (pts_idx) - p;
              vec2 line_tan;
              d = min (d, line_distance (p0, p1, line_tan));
              if (i > 1)
                d = min (d, join_distance (p0, p0_tan, line_tan));
              else
                start_tan = line_tan;
              p0_tan = line_tan;
              p0 = p1;
            }
            break;
        }
    }

  if (op == GSK_PATH_CLOSE)
    {
      d = min (d, join_distance (p0, p0_tan, start_tan));
    }
  else
    {
      d = min (d, cap_distance (start, start_tan));
      d = min (d, cap_distance (p0, - p0_tan));
    }

  path_idx = pts_idx;
}

float
stroke_distance (vec2 p)
{
  int path_idx = in_points_id;

  int n = get_int (path_idx);
  path_idx++;
  float d = 1.0;
  
  while (n-- > 0 && d > -1.0)
    {
      standard_contour_stroke_distance (p, path_idx, d);
    }

  return d;
}

float
stroke_coverage (vec2 p)
{
  float d = stroke_distance (p * scale);

  return clamp (0.5 - d, 0, 1);
}

void main()
{
  scale = max (push.scale.x, push.scale.y);
  line_width = in_half_line_width * scale;

  float alpha = in_color.a * rect_coverage (in_rect, in_pos);
  alpha *= stroke_coverage (in_pos / push.scale - in_offset);
  color = clip_scaled (in_pos, vec4(in_color.rgb, 1) * alpha);
}

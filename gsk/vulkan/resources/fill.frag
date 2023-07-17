#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

#define GSK_FILL_RULE_WINDING 0
#define GSK_FILL_RULE_EVEN_ODD 1

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
layout(location = 5) in flat int in_fill_rule;

layout(location = 0) out vec4 color;

vec2
get_point (inout int idx)
{
  vec2 p = vec2 (get_float (idx), get_float (idx + 1));
  idx += 2;
  p *= push.scale;

  return p;
}



float
cross2d (vec2 v0, vec2 v1)
{
  return v0.x * v1.y - v0.y * v1.x;
}

float
line_distance_squared (vec2 p,
                       vec2 start,
                       vec2 end,
                       inout int w)
{
  // distance
  vec2 e = end - start;
  vec2 v = p - start;
  vec2 pq = v - e * clamp (dot (v, e) / dot(e, e), 0.0, 1.0);
  float d = dot (pq, pq);

  // winding number from http://geomalgorithms.com/a03-_inclusion.html
  vec2 v2 = p - end;
  float val3 = cross2d (e, v); //isLeft
  bvec3 cond = bvec3 (v.y >= 0.0, v2.y < 0.0, val3 > 0.0);
  if (all (cond))
    w++; // have a valid up intersect
  else if (all (not (cond)))
    w--; // have a valid down intersect

  return d;
}



// Modified from http://tog.acm.org/resources/GraphicsGems/gems/Roots3And4.c
// Credits to Doublefresh for hinting there
int
solve_quadratic (vec2 coeffs,
                 inout vec2 roots)
{
  // normal form: x^2 + px + q = 0
  float p = coeffs[1] / 2.;
  float q = coeffs[0];

  float D = p * p - q;

  if (D < 0.)
    return 0;

  roots[0] = -sqrt (D) - p;
  roots[1] = sqrt (D) - p;

  return 2;
}

//From Trisomie21
//But instead of his cancellation fix i'm using a newton iteration
int solve_cubic(vec3 coeffs, inout vec3 r){

  float a = coeffs[2];
  float b = coeffs[1];
  float c = coeffs[0];

  float p = b - a*a / 3.0;
  float q = a * (2.0*a*a - 9.0*b) / 27.0 + c;
  float p3 = p*p*p;
  float d = q*q + 4.0*p3 / 27.0;
  float offset = -a / 3.0;
  if(d >= 0.0) { // Single solution
    float z = sqrt(d);
    float u = (-q + z) / 2.0;
    float v = (-q - z) / 2.0;
    u = sign(u)*pow(abs(u),1.0/3.0);
    v = sign(v)*pow(abs(v),1.0/3.0);
    r[0] = offset + u + v;  

    //Single newton iteration to account for cancellation
    float f = ((r[0] + a) * r[0] + b) * r[0] + c;
    float f1 = (3. * r[0] + 2. * a) * r[0] + b;

    r[0] -= f / f1;

    return 1;
  }
  float u = sqrt(-p / 3.0);
  float v = acos(-sqrt( -27.0 / p3) * q / 2.0) / 3.0;
  float m = cos(v), n = sin(v)*1.732050808;

  //Single newton iteration to account for cancellation
  //(once for every root)
  r[0] = offset + u * (m + m);
    r[1] = offset - u * (n + m);
    r[2] = offset + u * (n - m);

  vec3 f = ((r + a) * r + b) * r + c;
  vec3 f1 = (3. * r + 2. * a) * r + b;

  r -= f / f1;

  return 3;
}

float cubic_bezier_normal_iteration(float t, vec2 a0, vec2 a1, vec2 a2, vec2 a3){
  //horner's method
  vec2 a_2=a2+t*a3;
  vec2 a_1=a1+t*a_2;
  vec2 b_2=a_2+t*a3;

  vec2 uv_to_p=a0+t*a_1;
  vec2 tang=a_1+t*b_2;

  float l_tang=dot(tang,tang);
  return t-dot(tang,uv_to_p)/l_tang;
}

float
cubic_distance_squared (vec2 uv,
                        vec2 p0,
                        vec2 p1,
                        vec2 p2,
                        vec2 p3,
                        inout int w)
{
  vec2 a3 = (-p0 + 3. * p1 - 3. * p2 + p3);
  vec2 a2 = (3. * p0 - 6. * p1 + 3. * p2);
  vec2 a1 = (-3. * p0 + 3. * p1);
  vec2 a0 = p0 - uv;

  float d0 = 1e38;

  float t;
  vec3 params = vec3 (0, .5, 1);

  for (int i = 0; i < 3; i++)
    {
      t = params[i];
      for (int j = 0; j < 3; j++)
        {
          t = cubic_bezier_normal_iteration (t, a0, a1, a2, a3);
        }
      t = clamp (t, 0., 1.);
      vec2 uv_to_p = ((a3 * t + a2) * t + a1) * t + a0;
      d0 = min (d0, dot (uv_to_p, uv_to_p));
  }

  vec3 roots = vec3 (1e38);
  int n_roots;

  if (uv.x <= min (min (p0.x, p1.x), min (p2.x, p3.x)))
    {
      if (uv.y >= min (p0.y,p3.y) && uv.y <= max (p0.y, p3.y))
        {
          if (cross2d (p0 - uv, p3 - uv) > 0.0)
            w++;
          else
            w--;
        }
    }
  else
    {
      if (abs (a3.y) < .0001)
        n_roots = solve_quadratic (vec2 (a0.y / a2.y, a1.y / a2.y), roots.xy);
      else
        n_roots = solve_cubic (vec3 (a0.y / a3.y, a1.y / a3.y, a2.y / a3.y), roots);

      for (int i = 0; i < n_roots; i++)
        {
          if (roots[i] > 0. && roots[i] <= 1.)
            {
              if (((a3.x * roots[i] + a2.x) * roots[i] + a1.x) * roots[i] + a0.x > 0.0)
                {
                  if ((3 * a3.y * roots[i] + 2 * a2.y) * roots[i] + a1.y > 0.0)
                    w++;
                  else
                    w--;
                }
            }
        }
    }

  return d0;
}

float
standard_contour_distance (vec2      p,
                           inout int path_idx,
                           inout int w)
{
  int n_points = get_int (path_idx);
  int ops_idx = path_idx + 1;
  int pts_idx = ops_idx + n_points;

  vec2 start = get_point (pts_idx);
  float d = dot (p - start, p - start);
  int op = GSK_PATH_MOVE;
  
  for (int i = 1; i < n_points; i++)
    {
      op = get_int (ops_idx + i);
      switch (op)
        {
          case GSK_PATH_MOVE:
            discard;

          case GSK_PATH_CLOSE:
          case GSK_PATH_LINE:
            {
              vec2 end = get_point (pts_idx);
              d = min (d, line_distance_squared (p, start, end, w));
              start = end;
            }
            break;

          case GSK_PATH_CURVE:
            {
              vec2 p1 = get_point (pts_idx);
              vec2 p2 = get_point (pts_idx);
              vec2 p3 = get_point (pts_idx);
              //d = min (d, line_distance_squared (p, start, p3, w));
              d = min (d, cubic_distance_squared (p, start, p1, p2, p3, w));
              start = p3;
            }
            break;

          case GSK_PATH_CONIC:
            {
              pts_idx += 4;
              vec2 end = get_point (pts_idx);
              d = min (d, line_distance_squared (p, start, end, w));
              start = end;
            }
            break;
        }
    }

  if (op != GSK_PATH_CLOSE)
    {
      int tmp_idx = ops_idx + n_points;
      vec2 end = get_point (tmp_idx);
      d = min (d, line_distance_squared (p, start, end, w));
    }

  path_idx = pts_idx;

  return d;
}

float
path_distance (vec2 p)
{
  int path_idx = in_points_id;

  int n = get_int (path_idx);
  path_idx++;
  int w = 0;
  float d = 1e38;
  
  while (n-- > 0)
    {
       d = min (d, standard_contour_distance (p, path_idx, w));
    }

  if (in_fill_rule == GSK_FILL_RULE_WINDING)
    {
      if (w == 0)
        return sqrt (d);
      else if (abs (w) == 1)
        return - sqrt (d);
      else
        return -1.0; /* not really, but good enough */
    }
  else if (in_fill_rule == GSK_FILL_RULE_EVEN_ODD)
    {
      return sqrt (d) * (w % 2 == 0 ? 1.0 : -1.0);
    }
  else
    {
      discard;
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

#define GSK_N_TEXTURES 0

#include "common.glsl"

#define GSK_FILL_RULE_WINDING 0
#define GSK_FILL_RULE_EVEN_ODD 1

#define GSK_PATH_MOVE 0
#define GSK_PATH_CLOSE 1
#define GSK_PATH_LINE 2
#define GSK_PATH_CURVE 3
#define GSK_PATH_CONIC 4

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS_FLAT(2) vec4 _color;
PASS_FLAT(3) vec2 _offset;
PASS_FLAT(4) uint _points_id;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_color;
IN(2) vec2 in_offset;
IN(3) uint in_points_id;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);

  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;
  _color = output_color_from_alt (in_color);
  _offset = in_offset;
  _points_id = in_points_id;
}

#endif


#ifdef GSK_FRAGMENT_SHADER

vec2
get_point (inout uint idx)
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
line_distance_squared (vec2  start,
                       vec2  end,
                       float d0)
{
  vec2 e = end - start;
  vec2 v = - start;
  vec2 pq = v - e * clamp (dot (v, e) / dot(e, e), 0.0, 1.0);
  return dot (pq, pq);
}

int
line_winding (vec2 start,
              vec2 end)
{
  // winding number from http://geomalgorithms.com/a03-_inclusion.html
  float val3 = cross2d (start, end - start); //isLeft
  bvec3 cond = bvec3 (start.y <= 0.0, end.y > 0.0, val3 > 0.0);
  if (all (cond))
    return 1;
  else if (all (not (cond)))
    return -1;
  else
    return 0;
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

vec3
sort3 (vec3 v)
{
  vec2 t = vec2 ( min(v.x, v.y), max (v.x, v.y));
  return vec3 (min (t.x, v.z), min (max (v.z, t.x), t.y), max (t.y, v.z));
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
  if(d >= 0.0)
    { 
      // Single solution
      float z = sqrt(d);
      float u = (-q + z) / 2.0;
      float v = (-q - z) / 2.0;
      u = sign (u) * pow (abs (u), 1.0 / 3.0);
      v = sign (v) * pow (abs (v), 1.0 / 3.0);
      r[0] = offset + u + v;  

      //Single newton iteration to account for cancellation
      float f = ((r[0] + a) * r[0] + b) * r[0] + c;
      float f1 = (3. * r[0] + 2. * a) * r[0] + b;

      r[0] -= f / f1;

      return 1;
    }
  float u = sqrt (-p / 3.0);
  float v = acos (-sqrt ( -27.0 / p3) * q / 2.0) / 3.0;
  float m = cos (v), n = sin (v)*1.732050808;

  // Single newton iteration to account for cancellation
  // (once for every root)
  r[0] = offset + u * (m + m);
  r[1] = offset - u * (n + m);
  r[2] = offset + u * (n - m);

  vec3 f = ((r + a) * r + b) * r + c;
  vec3 f1 = (3. * r + 2. * a) * r + b;

  r -= f / f1;
  r = sort3 (r);

  return 3;
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
  vec2 tang = a_1 + t*b_2;

  float l_tang = dot (tang,tang);
  return t - dot (tang, uv_to_p) / l_tang;
}

float
cubic_distance_squared (vec2  p0,
                        vec2  p1,
                        vec2  p2,
                        vec2  p3,
                        float d0)
{
  if (dot (p0, p0) >= d0 &&
      dot (p1, p1) >= d0 &&
      dot (p2, p2) >= d0 &&
      dot (p3, p3) >= d0)
    return d0;

  vec2 a3 = (-p0 + 3. * p1 - 3. * p2 + p3);
  vec2 a2 = (3. * p0 - 6. * p1 + 3. * p2);
  vec2 a1 = (-3. * p0 + 3. * p1);
  vec2 a0 = p0;

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

  return d0;
}

int
cubic_winding (vec2 p0,
               vec2 p1,
               vec2 p2,
               vec2 p3)
{
  vec3 roots = vec3 (1e38);
  int n_roots;
  int w = 0;

  if ((p0.y > 0 && p1.y > 0 && p2.y > 0 && p3.y > 0) ||
      (p0.y <= 0 && p1.y <= 0 && p2.y <= 0 && p3.y <= 0))
    return 0;

  if (0 <= min (min (p0.x, p1.x), min (p2.x, p3.x)))
    {
      if (p0.y <= 0 && p3.y > 0)
        return 1;
      else if (p0.y > 0 && p3.y <= 0)
        return -1;
      else
        return 0;
    }

  vec2 a3 = (-p0 + 3. * p1 - 3. * p2 + p3);
  vec2 a2 = (3. * p0 - 6. * p1 + 3. * p2);
  vec2 a1 = (-3. * p0 + 3. * p1);
  vec2 a0 = p0;

  if (abs (a3.y) < max (max (a0.y, a1.y), a2.y) * .001)
    n_roots = solve_quadratic (vec2 (a0.y / a2.y, a1.y / a2.y), roots.xy);
  else
    n_roots = solve_cubic (vec3 (a0.y / a3.y, a1.y / a3.y, a2.y / a3.y), roots);

  bool greater = p0.y > 0.0;
  float last = 0;
  float last_x = p0.x;
  for (int i = 0; i < n_roots; i++)
    {
      if (roots[i] < 0. || roots[i] > 1.)
        continue;

      float t = 0.5 * (roots[i] + last);
      bool new_greater = fma (fma (fma (a3.y, t,  a2.y), t, a1.y), t, a0.y) > 0.0;
      if (new_greater != greater)
        {
          greater = new_greater;

          if (last_x > 0.0)
            w += (greater ? 1 : -1);
        }

      last_x = fma (fma (fma (a3.x, roots[i], a2.x), roots[i], a1.x), roots[i], a0.x);
      last = roots[i];
    }

  bool new_greater = p3.y > 0;
  if (new_greater != greater)
    {
      if (last_x > 0.0)
        w += (new_greater ? 1 : -1);
    }

  return w;
}

void
standard_contour_distance (vec2        p,
                           inout float d,
                           inout uint  path_idx,
                           inout int   w)
{
  int n_points = get_int (path_idx);
  uint ops_idx = path_idx + 1;
  uint pts_idx = ops_idx + n_points;

  vec2 start = get_point (pts_idx) - p;
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
              vec2 end = get_point (pts_idx) - p;
              d = line_distance_squared (start, end, d);
              w += line_winding (start, end);
              start = end;
            }
            break;

          case GSK_PATH_CURVE:
            {
              vec2 p1 = get_point (pts_idx) - p;
              vec2 p2 = get_point (pts_idx) - p;
              vec2 p3 = get_point (pts_idx) - p;
              d = cubic_distance_squared (start, p1, p2, p3, d);
              w += cubic_winding (start, p1, p2, p3);
              start = p3;
            }
            break;

          case GSK_PATH_CONIC:
            {
              pts_idx += 4;
              vec2 end = get_point (pts_idx) - p;
              d = line_distance_squared (start, end, d);
              w += line_winding (start, end);
              start = end;
            }
            break;
        }
    }

  if (op != GSK_PATH_CLOSE)
    {
      uint tmp_idx = ops_idx + n_points;
      vec2 end = get_point (tmp_idx) - p;
      d = line_distance_squared (start, end, d);
      w += line_winding (start, end);
    }

  path_idx = pts_idx;
}

float
path_distance (vec2 p, uint fill_rule)
{
  uint path_idx = _points_id;

  int n = get_int (path_idx);
  path_idx++;
  int w = 0;
  float d = 1e38;

  while (n-- > 0)
    {
       standard_contour_distance (p, d, path_idx, w);
    }

  if (fill_rule == GSK_FILL_RULE_WINDING)
    {
      if (w == 0)
        return sqrt (d);
      else if (abs (w) == 1)
        return - sqrt (d);
      else
        return -1.0; /* not really, but good enough */
    }
  else if (fill_rule == GSK_FILL_RULE_EVEN_ODD)
    {
      return sqrt (d) * (w % 2 == 0 ? 1.0 : -1.0);
    }
  else
    {
      discard;
    }
}

float
path_coverage (vec2 p, uint fill_rule)
{
  float d = path_distance (p, fill_rule);

  return clamp (0.5 - d, 0.0, 1.0);
}

void
run (out vec4 color,
     out vec2 position)
{
  float cov = path_coverage (_pos - _offset * push.scale, GSK_VARIATION);
  color = output_color_alpha (_color, _color.a * cov);
}

#endif

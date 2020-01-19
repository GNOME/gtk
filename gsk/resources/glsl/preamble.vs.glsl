uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform float u_alpha;

#if GSK_GLES
attribute vec2 aPosition;
attribute vec2 aUv;
_OUT_ vec2 vUv;
#elif GSK_LEGACY
attribute vec2 aPosition;
attribute vec2 aUv;
_OUT_ vec2 vUv;
#else
_IN_ vec2 aPosition;
_IN_ vec2 aUv;
_OUT_ vec2 vUv;
#endif

// amount is: top, right, bottom, left
RoundedRect
rounded_rect_shrink (RoundedRect r, vec4 amount)
{
  vec4 new_bounds = r.bounds + vec4(1.0,1.0,-1.0,-1.0) * amount.wxyz;
  vec4 new_corner_points1 = r.corner_points1;
  vec4 new_corner_points2 = r.corner_points2;

  if (r.corner_points1.xy == r.bounds.xy) new_corner_points1.xy = new_bounds.xy;
  if (r.corner_points1.zw == r.bounds.zy) new_corner_points1.zw = new_bounds.zy;
  if (r.corner_points2.xy == r.bounds.zw) new_corner_points2.xy = new_bounds.zw;
  if (r.corner_points2.zw == r.bounds.xw) new_corner_points2.zw = new_bounds.xw;

  return RoundedRect (new_bounds, new_corner_points1, new_corner_points2);
}

void
rounded_rect_offset(inout RoundedRect r, vec2 offset)
{
  r.bounds.xy += offset;
  r.bounds.zw += offset;
  r.corner_points1.xy += offset;
  r.corner_points1.zw += offset;
  r.corner_points2.xy += offset;
  r.corner_points2.zw += offset;
}

void rounded_rect_transform(inout RoundedRect r, mat4 mat)
{
  r.bounds.xy = (mat * vec4(r.bounds.xy, 0.0, 1.0)).xy;
  r.bounds.zw = (mat * vec4(r.bounds.zw, 0.0, 1.0)).xy;

  r.corner_points1.xy = (mat * vec4(r.corner_points1.xy, 0.0, 1.0)).xy;
  r.corner_points1.zw = (mat * vec4(r.corner_points1.zw, 0.0, 1.0)).xy;

  r.corner_points2.xy = (mat * vec4(r.corner_points2.xy, 0.0, 1.0)).xy;
  r.corner_points2.zw = (mat * vec4(r.corner_points2.zw, 0.0, 1.0)).xy;
}

#if defined(GSK_LEGACY)
// Can't have out or inout array parameters...
#define rounded_rect_encode(r, uni) uni[0] = r.bounds; uni[1] = r.corner_points1; uni[2] = r.corner_points2;
#else
void rounded_rect_encode(RoundedRect r, out _ROUNDED_RECT_UNIFORM_ out_r)
{
#if defined(GSK_GLES)
  out_r[0] = r.bounds;
  out_r[1] = r.corner_points1;
  out_r[2] = r.corner_points2;
#else
  out_r = r;
#endif
}

#endif

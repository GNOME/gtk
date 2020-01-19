#ifdef GSK_GL3
precision highp float;
#endif

#ifdef GSK_GLES
precision highp float;
#endif

uniform sampler2D u_source;
uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform float u_alpha;// = 1.0;
uniform vec4 u_viewport;
uniform vec4[3] u_clip_rect;

#if GSK_GLES
#define _OUT_ varying
#define _IN_ varying
#elif GSK_LEGACY
#define _OUT_ varying
#define _IN_ varying
_OUT_ vec4 outputColor;
#else
#define _OUT_ out
#define _IN_ in
_OUT_ vec4 outputColor;
#endif
_IN_ vec2 vUv;



struct RoundedRect
{
  vec4 bounds;
  // Look, arrays can't be in structs if you want to return the struct
  // from a function in gles or whatever. Just kill me.
  vec4 corner_points1; // xy = top left, zw = top right
  vec4 corner_points2; // xy = bottom right, zw = bottom left
};

// Transform from a GskRoundedRect to a RoundedRect as we need it.
RoundedRect
create_rect(vec4[3] data)
{
  vec4 bounds = vec4(data[0].xy, data[0].xy + data[0].zw);

  vec4 corner_points1 = vec4(bounds.xy + data[1].xy,
                             bounds.zy + vec2(data[1].zw * vec2(-1, 1)));
  vec4 corner_points2 = vec4(bounds.zw + (data[2].xy * vec2(-1, -1)),
                             bounds.xw + vec2(data[2].zw * vec2(1, -1)));

  return RoundedRect(bounds, corner_points1, corner_points2);
}

float
ellipsis_dist (vec2 p, vec2 radius)
{
  if (radius == vec2(0, 0))
    return 0.0;

  vec2 p0 = p / radius;
  vec2 p1 = 2.0 * p0 / radius;

  return (dot(p0, p0) - 1.0) / length (p1);
}

float
ellipsis_coverage (vec2 point, vec2 center, vec2 radius)
{
  float d = ellipsis_dist (point - center, radius);
  return clamp (0.5 - d, 0.0, 1.0);
}

float
rounded_rect_coverage (RoundedRect r, vec2 p)
{
  if (p.x < r.bounds.x || p.y < r.bounds.y ||
      p.x >= r.bounds.z || p.y >= r.bounds.w)
    return 0.0;

  vec2 rad_tl = r.corner_points1.xy - r.bounds.xy;
  vec2 rad_tr = r.corner_points1.zw - r.bounds.zy;
  vec2 rad_br = r.corner_points2.xy - r.bounds.zw;
  vec2 rad_bl = r.corner_points2.zw - r.bounds.xw;

  vec2 ref_tl = r.corner_points1.xy;
  vec2 ref_tr = r.corner_points1.zw;
  vec2 ref_br = r.corner_points2.xy;
  vec2 ref_bl = r.corner_points2.zw;

  float d_tl = ellipsis_coverage(p, ref_tl, rad_tl);
  float d_tr = ellipsis_coverage(p, ref_tr, rad_tr);
  float d_br = ellipsis_coverage(p, ref_br, rad_br);
  float d_bl = ellipsis_coverage(p, ref_bl, rad_bl);

  vec4 corner_coverages = 1.0 - vec4(d_tl, d_tr, d_br, d_bl);

  bvec4 is_out = bvec4(p.x < ref_tl.x && p.y < ref_tl.y,
                       p.x > ref_tr.x && p.y < ref_tr.y,
                       p.x > ref_br.x && p.y > ref_br.y,
                       p.x < ref_bl.x && p.y > ref_bl.y);

  return 1.0 - dot(vec4(is_out), corner_coverages);
}

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

vec4 Texture(sampler2D sampler, vec2 texCoords) {
#if GSK_GLES
  return texture2D(sampler, texCoords);
#elif GSK_LEGACY
  return texture2D(sampler, texCoords);
#else
  return texture(sampler, texCoords);
#endif
}

void setOutputColor(vec4 color) {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;


  // We do *NOT* transform the clip rect here since we already
  // need to do that on the CPU.
#if GSK_GLES
  gl_FragColor = color * rounded_rect_coverage(create_rect(u_clip_rect), f.xy);
#elif GSK_LEGACY
  gl_FragColor = color * rounded_rect_coverage(create_rect(u_clip_rect), f.xy);
#else
  outputColor = color * rounded_rect_coverage(create_rect(u_clip_rect), f.xy);
#endif
  /*outputColor = color;*/
}

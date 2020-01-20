uniform sampler2D u_source;
uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform float u_alpha;// = 1.0;
uniform vec4 u_viewport;
uniform vec4[3] u_clip_rect;

#if GSK_GLES
#elif GSK_LEGACY
_OUT_ vec4 outputColor;
#else
_OUT_ vec4 outputColor;
#endif

_IN_ vec2 vUv;



RoundedRect decode_rect(_ROUNDED_RECT_UNIFORM_ r)
{
#if defined(GSK_GLES) || defined(GSK_LEGACY)
  return RoundedRect(r[0], r[1], r[2]);
#else
  return r;
#endif
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

vec4 Texture(sampler2D sampler, vec2 texCoords) {
#if defined(GSK_GLES) || defined(GSK_LEGACY)
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
#if defined(GSK_GLES) || defined(GSK_LEGACY)
  gl_FragColor = color * rounded_rect_coverage(create_rect(u_clip_rect), f.xy);
#else
  outputColor = color * rounded_rect_coverage(create_rect(u_clip_rect), f.xy);
#endif
  /*outputColor = color;*/
}

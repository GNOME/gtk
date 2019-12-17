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
  vec4 corner_widths;
  vec4 corner_heights;
};

// Transform from a GskRoundedRect to a RoundedRect as we need it.
RoundedRect
create_rect(vec4 data[3])
{
  vec4 bounds = vec4(data[0].xy, data[0].xy + data[0].zw);
  vec4 widths = vec4(data[1].x, data[1].z, data[2].x, data[2].z);
  vec4 heights = vec4(data[1].y, data[1].w, data[2].y, data[2].w);
  return RoundedRect(bounds, widths, heights);
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

  vec2 rad_tl = vec2(r.corner_widths.x, r.corner_heights.x);
  vec2 rad_tr = vec2(r.corner_widths.y, r.corner_heights.y);
  vec2 rad_br = vec2(r.corner_widths.z, r.corner_heights.z);
  vec2 rad_bl = vec2(r.corner_widths.w, r.corner_heights.w);

  vec2 ref_tl = r.bounds.xy + vec2( r.corner_widths.x,  r.corner_heights.x);
  vec2 ref_tr = r.bounds.zy + vec2(-r.corner_widths.y,  r.corner_heights.y);
  vec2 ref_br = r.bounds.zw + vec2(-r.corner_widths.z, -r.corner_heights.z);
  vec2 ref_bl = r.bounds.xw + vec2( r.corner_widths.w, -r.corner_heights.w);

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
  vec4 new_widths = vec4(0);
  vec4 new_heights = vec4(0);

  // Left top
  if (r.corner_widths.x  > 0.0) new_widths.x = r.corner_widths.x - amount.w;
  if (r.corner_heights.x > 0.0) new_heights.x = r.corner_heights.x - amount.x;

  // Top right
  if (r.corner_widths.y  > 0.0) new_widths.y = r.corner_widths.y - amount.y;
  if (r.corner_heights.y > 0.0) new_heights.y = r.corner_heights.y - amount.x;

  // Bottom right
  if (r.corner_widths.z  > 0.0) new_widths.z = r.corner_widths.z - amount.y;
  if (r.corner_heights.z > 0.0) new_heights.z = r.corner_heights.z - amount.z;

  // Bottom left
  if (r.corner_widths.w  > 0.0) new_widths.w = r.corner_widths.w - amount.w;
  if (r.corner_heights.w > 0.0) new_heights.w = r.corner_heights.w - amount.z;

  return RoundedRect (new_bounds, new_widths, new_heights);
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

#if GSK_GLES
  gl_FragColor = color * rounded_rect_coverage(create_rect(u_clip_rect), f.xy);
#elif GSK_LEGACY
  gl_FragColor = color * rounded_rect_coverage(create_rect(u_clip_rect), f.xy);
#else
  outputColor = color * rounded_rect_coverage(create_rect(u_clip_rect), f.xy);
#endif
  /*outputColor = color;*/
}

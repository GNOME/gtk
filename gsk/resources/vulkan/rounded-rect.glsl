#ifndef _ROUNDED_RECT_
#define _ROUNDED_RECT_

struct RoundedRect
{
  vec4 bounds;
  vec4 corner_widths;
  vec4 corner_heights;
};

float
ellipsis_dist (vec2 p, vec2 radius)
{
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

RoundedRect
rounded_rect_shrink (RoundedRect r, vec4 amount)
{
  vec4 new_bounds = r.bounds + vec4(1.0,1.0,-1.0,-1.0) * amount.wxyz;
  vec4 new_widths = max (r.corner_widths - amount.wyyw, 0.0);
  vec4 new_heights = max (r.corner_heights - amount.xxzz, 0.0);
              
  return RoundedRect (new_bounds, new_widths, new_heights);
}

#endif

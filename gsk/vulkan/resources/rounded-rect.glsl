#ifndef _ROUNDED_RECT_
#define _ROUNDED_RECT_

#include "ellipse.glsl"
#include "rect.glsl"

struct RoundedRect
{
  vec4 bounds;
  vec4 corner_widths;
  vec4 corner_heights;
};

float
rounded_rect_distance (RoundedRect r, vec2 p)
{
  Rect bounds = Rect(vec4(r.bounds));

  float bounds_distance = rect_distance (bounds, p);
                      
  Ellipse tl = Ellipse (r.bounds.xy + vec2( r.corner_widths.x,  r.corner_heights.x),
                        vec2(r.corner_widths.x, r.corner_heights.x));
  Ellipse tr = Ellipse (r.bounds.zy + vec2(-r.corner_widths.y,  r.corner_heights.y),
                        vec2(r.corner_widths.y, r.corner_heights.y));
  Ellipse br = Ellipse (r.bounds.zw + vec2(-r.corner_widths.z, -r.corner_heights.z),
                        vec2(r.corner_widths.z, r.corner_heights.z));
  Ellipse bl = Ellipse (r.bounds.xw + vec2( r.corner_widths.w, -r.corner_heights.w),
                        vec2(r.corner_widths.w, r.corner_heights.w));

 vec4 distances = vec4(ellipse_distance (tl, p),
                       ellipse_distance (tr, p),
                       ellipse_distance (br, p),
                       ellipse_distance (bl, p));

  bvec4 is_out = bvec4(p.x < tl.center.x && p.y < tl.center.y,
                       p.x > tr.center.x && p.y < tr.center.y,
                       p.x > br.center.x && p.y > br.center.y,
                       p.x < bl.center.x && p.y > bl.center.y);
  distances = mix (vec4(bounds_distance), distances, is_out);

  vec2 max2 = max (distances.xy, distances.zw);
  return max (max2.x, max2.y);
}

RoundedRect
rounded_rect_scale (RoundedRect r, vec2 scale)
{
  r.bounds *= scale.xyxy;
  r.corner_widths *= scale.xxxx;
  r.corner_heights *= scale.yyyy;

  return r;
}

RoundedRect
rounded_rect_shrink (RoundedRect r, vec4 amount)
{
  vec4 new_bounds = r.bounds + vec4(1.0,1.0,-1.0,-1.0) * amount.wxyz;
  vec4 new_widths = max (r.corner_widths - sign (r.corner_widths) * amount.wyyw, 0.0);
  vec4 new_heights = max (r.corner_heights - sign (r.corner_heights) * amount.xxzz, 0.0);
              
  return RoundedRect (new_bounds, new_widths, new_heights);
}

#endif

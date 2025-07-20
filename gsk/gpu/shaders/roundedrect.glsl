#ifndef _ROUNDED_RECT_
#define _ROUNDED_RECT_

#include "ellipse.glsl"
#include "rect.glsl"

#define RoundedRect vec4[3]

RoundedRect
rounded_rect_from_rect (Rect r)
{
  return RoundedRect (rect_bounds(r), vec4 (0.0), vec4 (0.0));
}

RoundedRect
rounded_rect_from_gsk (mat3x4 gsk_rounded_rect)
{
  return RoundedRect ((gsk_rounded_rect[0].xyxy + vec4 (0.0, 0.0, gsk_rounded_rect[0].zw)) * GSK_GLOBAL_SCALE.xyxy,
                      gsk_rounded_rect[1] * GSK_GLOBAL_SCALE.xxxx,
                      gsk_rounded_rect[2] * GSK_GLOBAL_SCALE.yyyy);
}

vec4
rounded_rect_bounds (RoundedRect r)
{
  return r[0];
}

vec4
rounded_rect_corner_widths (RoundedRect r)
{
  return r[1];
}

vec4
rounded_rect_corner_heights (RoundedRect r)
{
  return r[2];
}

vec2
rounded_rect_corner (RoundedRect r, const uint corner)
{
  return vec2 (
    rounded_rect_corner_widths (r)[corner],
    rounded_rect_corner_heights (r)[corner]
  );
}

float
rounded_rect_corner_width (RoundedRect r, const uint corner)
{
  return rounded_rect_corner_widths (r)[corner];
}

float
rounded_rect_corner_height (RoundedRect r, const uint corner)
{
  return rounded_rect_corner_heights (r)[corner];
}

float
rounded_rect_distance (RoundedRect r, vec2 p)
{
  float bounds_distance = rect_distance (Rect (rounded_rect_bounds (r)), p);
                      
  Ellipse tl = Ellipse (rounded_rect_bounds (r).xy + vec2( 1.,  1.)*rounded_rect_corner (r, TOP_LEFT),
                        rounded_rect_corner (r, TOP_LEFT));
  Ellipse tr = Ellipse (rounded_rect_bounds (r).zy + vec2(-1.,  1.)*rounded_rect_corner (r, TOP_RIGHT),
                        rounded_rect_corner (r, TOP_RIGHT));
  Ellipse br = Ellipse (rounded_rect_bounds (r).zw + vec2(-1., -1.)*rounded_rect_corner (r, BOTTOM_RIGHT),
                        rounded_rect_corner (r, BOTTOM_RIGHT));
  Ellipse bl = Ellipse (rounded_rect_bounds (r).xw + vec2( 1., -1.)*rounded_rect_corner (r, BOTTOM_LEFT),
                        rounded_rect_corner (r, BOTTOM_LEFT));

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
rounded_rect_shrink (RoundedRect r, vec4 amount)
{
  vec4 new_bounds = rounded_rect_bounds (r) + vec4(1.0,1.0,-1.0,-1.0) * amount.wxyz;
  vec4 new_widths = max (rounded_rect_corner_widths (r) - sign (rounded_rect_corner_widths (r)) * amount.wyyw, 0.0);
  vec4 new_heights = max (rounded_rect_corner_heights (r) - sign (rounded_rect_corner_heights (r)) * amount.xxzz, 0.0);
  new_widths = min (new_widths, new_bounds.z - new_bounds.x);
  new_heights = min (new_heights, new_bounds.w - new_bounds.y);
              
  return RoundedRect (new_bounds, new_widths, new_heights);
}

void
rounded_rect_scale (inout RoundedRect r,
                    vec2              scale)
{
  r = RoundedRect (
    rounded_rect_bounds (r) * scale.xyxy,
    rounded_rect_corner_widths (r) * scale.xxxx,
    rounded_rect_corner_heights (r) * scale.yyyy
  );
}

void
rounded_rect_offset (inout RoundedRect r,
                     vec2              offset)
{
  r = RoundedRect (
    rounded_rect_bounds (r) + offset.xyxy,
    rounded_rect_corner_widths (r),
    rounded_rect_corner_heights (r)
  );
}

bool
rounded_rect_is_slicable (RoundedRect r)
{
  vec2 size = rect_size (Rect (rounded_rect_bounds (r)));
  return any (lessThanEqual (rounded_rect_corner (r, TOP_LEFT) + rounded_rect_corner (r, BOTTOM_RIGHT), size))
      && any (lessThanEqual (rounded_rect_corner (r, BOTTOM_LEFT) + rounded_rect_corner (r, TOP_RIGHT), size));
}

Rect
rounded_rect_intersection_fallback_slice (RoundedRect outside,
                                          RoundedRect inside,
                                          uint        slice)
{
  switch (slice)
    {
    default:
    case SLICE_TOP:
    case SLICE_RIGHT:
    case SLICE_BOTTOM:
    case SLICE_LEFT:
      return Rect (vec4 (0.0));

    case SLICE_TOP_LEFT:
      return Rect (vec4 (rounded_rect_bounds (outside).xy, 0.5 * (rounded_rect_bounds (outside).xy + rounded_rect_bounds (outside).zw)));

    case SLICE_TOP_RIGHT:
      return Rect (vec4 (0.5 * (rounded_rect_bounds (outside).x + rounded_rect_bounds (outside).z), rounded_rect_bounds (outside).y,
                         rounded_rect_bounds (outside).z, 0.5 * (rounded_rect_bounds (outside).y + rounded_rect_bounds (outside).w)));

    case SLICE_BOTTOM_RIGHT:
      return Rect (vec4 (0.5 * (rounded_rect_bounds (outside).xy + rounded_rect_bounds (outside).zw), rounded_rect_bounds (outside).zw));

    case SLICE_BOTTOM_LEFT:
      return Rect (vec4 (rounded_rect_bounds (outside).x, 0.5 * (rounded_rect_bounds (outside).y + rounded_rect_bounds (outside).w),
                         0.5 * (rounded_rect_bounds (outside).x + rounded_rect_bounds (outside).z), rounded_rect_bounds (outside).w));
    }
}

Rect
rounded_rect_intersection_slice (RoundedRect outside,
                                 RoundedRect inside,
                                 uint        slice)
{
  float left, right, top, bottom;

  if (!rounded_rect_is_slicable (outside) ||
      !rounded_rect_is_slicable (inside))
    return rounded_rect_intersection_fallback_slice (outside, inside, slice);

  switch (slice)
    {
    case SLICE_TOP_LEFT:
      return Rect (vec4 (rounded_rect_bounds (outside).x,
                         rounded_rect_bounds (outside).y,
                         max (rounded_rect_bounds (outside).x + rounded_rect_corner_width (outside, TOP_LEFT),
                              rounded_rect_bounds (inside).x + rounded_rect_corner_width (inside, TOP_LEFT)),
                         max (rounded_rect_bounds (outside).y + rounded_rect_corner_height (outside, TOP_LEFT),
                              rounded_rect_bounds (inside).y + rounded_rect_corner_height (inside, TOP_LEFT))));

    case SLICE_TOP:
      left = max (rounded_rect_bounds (outside).x + rounded_rect_corner_width (outside, TOP_LEFT),
                  rounded_rect_bounds (inside).x + rounded_rect_corner_width (inside, TOP_LEFT));
      right = min (rounded_rect_bounds (outside).z - rounded_rect_corner_width (outside, TOP_RIGHT),
                   rounded_rect_bounds (inside).z - rounded_rect_corner_width (inside, TOP_RIGHT));
      return Rect (vec4 (left,
                         rounded_rect_bounds (outside).y,
                         max (left, right),
                         max (rounded_rect_bounds (outside).y, rounded_rect_bounds (inside).y)));

    case SLICE_TOP_RIGHT:
      left = max (min (rounded_rect_bounds (outside).z - rounded_rect_corner_width (outside, TOP_RIGHT),
                       rounded_rect_bounds (inside).z - rounded_rect_corner_width (inside, TOP_RIGHT)),
                  max (rounded_rect_bounds (outside).x + rounded_rect_corner_width (outside, TOP_LEFT),
                       rounded_rect_bounds (inside).x + rounded_rect_corner_width (inside, TOP_LEFT)));
      return Rect (vec4 (left,
                         rounded_rect_bounds (outside).y,
                         rounded_rect_bounds (outside).z,
                         max (rounded_rect_bounds (outside).y + rounded_rect_corner_height (outside, TOP_RIGHT),
                              rounded_rect_bounds (inside).y + rounded_rect_corner_height (inside, TOP_RIGHT))));

    case SLICE_RIGHT:
      top = max (rounded_rect_bounds (outside).y + rounded_rect_corner_height (outside, TOP_RIGHT),
                 rounded_rect_bounds (inside).y + rounded_rect_corner_height (inside, TOP_RIGHT));
      bottom = min (rounded_rect_bounds (outside).w - rounded_rect_corner_height (outside, BOTTOM_RIGHT),
                    rounded_rect_bounds (inside).w - rounded_rect_corner_height (inside, BOTTOM_RIGHT));
      return Rect (vec4 (min (rounded_rect_bounds (outside).z, rounded_rect_bounds (inside).z),
                         top,
                         rounded_rect_bounds (outside).z,
                         max (bottom, top)));

    case SLICE_BOTTOM_RIGHT:
      left = max (min (rounded_rect_bounds (outside).z - rounded_rect_corner_width (outside, BOTTOM_RIGHT),
                       rounded_rect_bounds (inside).z - rounded_rect_corner_width (inside, BOTTOM_RIGHT)),
                  max (rounded_rect_bounds (outside).x + rounded_rect_corner_width (outside, BOTTOM_LEFT),
                       rounded_rect_bounds (inside).x + rounded_rect_corner_width (inside, BOTTOM_LEFT)));
      bottom = max (min (rounded_rect_bounds (outside).w - rounded_rect_corner_height (outside, BOTTOM_RIGHT),
                         rounded_rect_bounds (inside).w - rounded_rect_corner_height (inside, BOTTOM_RIGHT)),
                    max (rounded_rect_bounds (outside).y + rounded_rect_corner_height (outside, TOP_RIGHT),
                         rounded_rect_bounds (inside).y + rounded_rect_corner_height (inside, TOP_RIGHT)));
      return Rect (vec4 (left,
                         bottom,
                         rounded_rect_bounds (outside).z,
                         rounded_rect_bounds (outside).w));

    case SLICE_BOTTOM:
      left = max (rounded_rect_bounds (outside).x + rounded_rect_corner_width (outside, BOTTOM_LEFT),
                  rounded_rect_bounds (inside).x + rounded_rect_corner_width (inside, BOTTOM_LEFT));
      right = min (rounded_rect_bounds (outside).z - rounded_rect_corner_width (outside, BOTTOM_RIGHT),
                   rounded_rect_bounds (inside).z - rounded_rect_corner_width (inside, BOTTOM_RIGHT));
      return Rect (vec4 (left,
                         min (rounded_rect_bounds (outside).w, rounded_rect_bounds (inside).w),
                         max (left, right),
                         rounded_rect_bounds (outside).w));

    case SLICE_BOTTOM_LEFT:
      bottom = max (min (rounded_rect_bounds (outside).w - rounded_rect_corner_height (outside, BOTTOM_LEFT),
                         rounded_rect_bounds (inside).w - rounded_rect_corner_height (inside, BOTTOM_LEFT)),
                    max (rounded_rect_bounds (outside).y + rounded_rect_corner_height (outside, TOP_LEFT),
                         rounded_rect_bounds (inside).y + rounded_rect_corner_height (inside, TOP_LEFT)));
      return Rect (vec4 (rounded_rect_bounds (outside).x,
                         bottom,
                         max (rounded_rect_bounds (outside).x + rounded_rect_corner_width (outside, BOTTOM_LEFT),
                              rounded_rect_bounds (inside).x + rounded_rect_corner_width (inside, BOTTOM_LEFT)),
                         rounded_rect_bounds (outside).w));

    case SLICE_LEFT:
      top = max (rounded_rect_bounds (outside).y + rounded_rect_corner_height (outside, TOP_LEFT),
                 rounded_rect_bounds (inside).y + rounded_rect_corner_height (inside, TOP_LEFT));
      bottom = min (rounded_rect_bounds (outside).w - rounded_rect_corner_height (outside, BOTTOM_LEFT),
                    rounded_rect_bounds (inside).w - rounded_rect_corner_height (inside, BOTTOM_LEFT));
      return Rect (vec4 (rounded_rect_bounds (outside).x,
                         top,
                         max (rounded_rect_bounds (outside).x, rounded_rect_bounds (inside).x),
                         max (bottom, top)));

    default:
      return Rect (vec4 (0.0));
    }
}

#ifdef GSK_FRAGMENT_SHADER

float
rounded_rect_coverage (RoundedRect r, vec2 p)
{
  vec2 fw = fwidth (p);
  float distance_scale = max (fw.x, fw.y);
  float distance = rounded_rect_distance (r, p) / distance_scale;
  float coverage = 0.5 - distance;

  return clamp (coverage, 0.0, 1.0);
}

#endif

#endif

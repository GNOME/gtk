precision highp float;

uniform sampler2D u_source;
uniform mat4 u_projection = mat4(1.0);
uniform mat4 u_modelview = mat4(1.0);
uniform float u_alpha = 1.0;
uniform vec4 u_viewport;

struct RoundedRect
{
  vec4 bounds;
  vec2 corners[4];
};

uniform RoundedRect u_clip_rect;

in vec2 vUv;
out vec4 outputColor;

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
      p.x >= (r.bounds.x + r.bounds.z) || p.y >= (r.bounds.y + r.bounds.w))
    return 0.0;

  vec2 rad_tl = r.corners[0];
  vec2 rad_tr = r.corners[1];
  vec2 rad_br = r.corners[2];
  vec2 rad_bl = r.corners[3];

  vec2 ref_tl = r.bounds.xy + r.corners[0];
  vec2 ref_tr = vec2(r.bounds.x + r.bounds.z, r.bounds.y) + (r.corners[1] * vec2(-1, 1));
  vec2 ref_br = vec2(r.bounds.x + r.bounds.z, r.bounds.y + r.bounds.w) - r.corners[2];
  vec2 ref_bl = vec2(r.bounds.x, r.bounds.y + r.bounds.w) + (r.corners[3] * vec2(1, -1));

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
  vec4 new_bounds = r.bounds;
  vec2 new_corners[4];

  new_bounds.xy += amount.wx;
  new_bounds.zw -= amount.wx + amount.yz;

  new_corners[0] = vec2(0);
  new_corners[1] = vec2(0);
  new_corners[2] = vec2(0);
  new_corners[3] = vec2(0);

  // Left top
  if (r.corners[0].x > 0 || r.corners[0].y > 0)
    new_corners[0] = r.corners[0] - amount.wx;

  // top right
  if (r.corners[1].x > 0 || r.corners[1].y > 0)
    new_corners[1] = r.corners[1] - amount.yx;

  // Bottom right
  if (r.corners[2].x > 0 || r.corners[2].y > 0)
    new_corners[2] = r.corners[2] - amount.yz;

  // Bottom left
  if (r.corners[3].x > 0 || r.corners[3].y > 0)
    new_corners[3] = r.corners[3] - amount.wz;

  return RoundedRect (new_bounds, new_corners);
}

vec4 Texture(sampler2D sampler, vec2 texCoords) {
  return texture(sampler, texCoords);
}

void setOutputColor(vec4 color) {
  vec4 f = gl_FragCoord;

  f.x += u_viewport.x;
  f.y = (u_viewport.y + u_viewport.w) - f.y;

  outputColor = color * rounded_rect_coverage(u_clip_rect, f.xy);
  /*outputColor = color;*/
}

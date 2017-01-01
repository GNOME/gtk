#version 420 core

struct RoundedRect {
  vec4 bounds;
  vec4 corners;
};

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in flat vec4 inClipBounds;
layout(location = 3) in flat vec4 inClipWidths;

layout(set = 0, binding = 0) uniform sampler2D inTexture;

layout(location = 0) out vec4 color;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

float clip(vec2 pos, RoundedRect r) {
  vec2 ref_tl = r.bounds.xy + vec2( r.corners.x,  r.corners.x);
  vec2 ref_tr = r.bounds.zy + vec2(-r.corners.y,  r.corners.y);
  vec2 ref_br = r.bounds.zw + vec2(-r.corners.z, -r.corners.z);
  vec2 ref_bl = r.bounds.xw + vec2( r.corners.w, -r.corners.w);
  
  float d_tl = distance(pos, ref_tl);
  float d_tr = distance(pos, ref_tr);
  float d_br = distance(pos, ref_br);
  float d_bl = distance(pos, ref_bl);

  float pixels_per_fragment = length(fwidth(pos.xy));
  float nudge = 0.5 * pixels_per_fragment;
  vec4 distances = vec4(d_tl, d_tr, d_br, d_bl) - r.corners + nudge;

  bvec4 is_out = bvec4(pos.x < ref_tl.x && pos.y < ref_tl.y,
                       pos.x > ref_tr.x && pos.y < ref_tr.y,
                       pos.x > ref_br.x && pos.y > ref_br.y,
                       pos.x < ref_bl.x && pos.y > ref_bl.y);

  float distance_from_border = dot(vec4(is_out),
                                   max(vec4(0.0, 0.0, 0.0, 0.0), distances));

  // Move the distance back into pixels.
  distance_from_border /= pixels_per_fragment;
  // Apply a more gradual fade out to transparent.
  //distance_from_border -= 0.5;

  return 1.0 - smoothstep(0.0, 1.0, distance_from_border);
}

void main()
{
  RoundedRect r = RoundedRect(vec4(inClipBounds.xy, inClipBounds.xy + inClipBounds.zw), inClipWidths);

  color = texture (inTexture, inTexCoord) * clip (inPos, r);
}

#version 450

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 in_source_rect;
layout(location = 1) in vec4 in_source_tex_rect;
layout(location = 2) in uint in_source_id;
layout(location = 3) in vec4 in_mask_rect;
layout(location = 4) in vec4 in_mask_tex_rect;
layout(location = 5) in uint in_mask_id;
layout(location = 6) in uint in_mask_mode;

layout(location = 0) out vec2 out_pos;
layout(location = 1) out flat Rect out_source_rect;
layout(location = 2) out vec2 out_source_coord;
layout(location = 3) out flat uint out_source_id;
layout(location = 4) out flat Rect out_mask_rect;
layout(location = 5) out vec2 out_mask_coord;
layout(location = 6) out flat uint out_mask_id;
layout(location = 7) out flat uint out_mask_mode;


void main() {
  Rect sr = rect_from_gsk (in_source_rect);
  Rect mr = rect_from_gsk (in_mask_rect);
  Rect r;
  if (in_mask_mode == 1)
    r = rect_union (sr, mr);
  else
    r = rect_intersect (sr, mr);

  vec2 pos = set_position_from_rect (r);

  out_pos = pos;
  out_source_rect = sr;
  out_source_coord = scale_tex_coord (pos, sr, in_source_tex_rect);
  out_source_id = in_source_id;
  out_mask_rect = mr;
  out_mask_coord = scale_tex_coord (pos, mr, in_mask_tex_rect);
  out_mask_id = in_mask_id;
  out_mask_mode = in_mask_mode;
}

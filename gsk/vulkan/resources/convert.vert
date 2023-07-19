#version 450

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 in_rect;
layout(location = 1) in vec4 in_tex_rect;
layout(location = 2) in uint in_tex_id;
layout(location = 3) in uint in_postprocess;

layout(location = 0) out vec2 out_pos;
layout(location = 1) out flat Rect out_rect;
layout(location = 2) out vec2 out_tex_coord;
layout(location = 3) out flat uint out_tex_id;
layout(location = 4) out flat uint out_postprocess;

void main() {
  Rect r = rect_from_gsk (in_rect);
  vec2 pos = set_position_from_rect (r);

  out_pos = pos;
  out_rect = r;
  out_tex_coord = scale_tex_coord (pos, r, in_tex_rect);
  out_tex_id = in_tex_id;
  out_postprocess = in_postprocess;
}

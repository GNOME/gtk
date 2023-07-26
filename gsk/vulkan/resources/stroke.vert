#version 450

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 in_rect;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_offset;
layout(location = 3) in int in_points_id;
layout(location = 4) in float in_line_width;
layout(location = 5) in int in_line_cap;
layout(location = 6) in int in_line_join;
layout(location = 7) in float in_miter_limit;

layout(location = 0) out vec2 out_pos;
layout(location = 1) out flat Rect out_rect;
layout(location = 2) out flat vec4 out_color;
layout(location = 3) out flat vec2 out_offset;
layout(location = 4) out flat int out_points_id;
layout(location = 5) out flat float out_half_line_width;
layout(location = 6) out flat int out_line_cap;
layout(location = 7) out flat int out_line_join;
layout(location = 8) out flat float out_miter_limit;

void main() {
  Rect r = rect_from_gsk (in_rect + vec4(in_offset, 0.0, 0.0));
  vec2 pos = set_position_from_rect (r);
  out_pos = pos;
  out_rect = r;
  out_color = in_color;
  out_offset = in_offset;
  out_points_id = in_points_id;
  out_half_line_width = 0.5 * in_line_width;
  out_line_cap = in_line_cap;
  out_line_join = in_line_join;
  out_miter_limit = in_miter_limit;
}

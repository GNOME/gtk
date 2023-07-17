#version 450

#include "common.vert.glsl"
#include "rect.vert.glsl"

layout(location = 0) in vec4 in_rect;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_offset;
layout(location = 3) in int in_points_id;
layout(location = 4) in int in_fill_rule;

layout(location = 0) out vec2 out_pos;
layout(location = 1) out flat Rect out_rect;
layout(location = 2) out flat vec4 out_color;
layout(location = 3) out flat vec2 out_offset;
layout(location = 4) out flat int out_points_id;
layout(location = 5) out flat int out_fill_rule;

void main() {
  Rect r = rect_from_gsk (in_rect + vec4(in_offset, 0.0, 0.0));
  vec2 pos = set_position_from_rect (r);
  out_pos = pos;
  out_rect = r;
  out_color = in_color;
  out_offset = in_offset;
  out_points_id = in_points_id;
  out_fill_rule = in_fill_rule;
}

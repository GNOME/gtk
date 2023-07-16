#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

layout(location = 0) in vec2 in_pos;
layout(location = 1) in flat Rect in_source_rect;
layout(location = 2) in vec2 in_source_coord;
layout(location = 3) in flat uint in_source_id;
layout(location = 4) in flat Rect in_mask_rect;
layout(location = 5) in vec2 in_mask_coord;
layout(location = 6) in flat uint in_mask_id;
layout(location = 7) in flat uint in_mask_mode;

layout(location = 0) out vec4 color;

float
luminance (vec3 color)
{
  return dot (vec3 (0.2126, 0.7152, 0.0722), color);
}

void main()
{
  vec4 source = texture(get_sampler (in_source_id), in_source_coord);
  source *= rect_coverage (in_source_rect, in_pos);
  vec4 mask = texture(get_sampler (in_mask_id), in_mask_coord);
  mask *= rect_coverage (in_mask_rect, in_pos);

  float alpha;
  if (in_mask_mode == 0)
    alpha = mask.a;
  else if (in_mask_mode == 1)
    alpha = 1.0 - mask.a;
  else if (in_mask_mode == 2)
    alpha = luminance (mask.rgb);
  else if (in_mask_mode == 3)
    alpha = mask.a - luminance (mask.rgb);

  color = clip_scaled (in_pos, source * alpha);
}

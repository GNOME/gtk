#version 450

#include "common.frag.glsl"
#include "clip.frag.glsl"
#include "rect.frag.glsl"

#define GSK_VULKAN_IMAGE_PREMULTIPLY (1 << 0)
#
layout(location = 0) in vec2 in_pos;
layout(location = 1) in Rect in_rect;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) flat in uint in_tex_id;
layout(location = 4) in flat uint in_postprocess;

layout(location = 0) out vec4 color;

void main()
{
  float alpha = rect_coverage (in_rect, in_pos);

  /* warning: This breaks with filters other than nearest,
     as linear filtering needs premultiplied alpha */
  vec4 pixel = texture (get_sampler (in_tex_id), in_tex_coord);

  if ((in_postprocess & GSK_VULKAN_IMAGE_PREMULTIPLY) != 0)
    pixel.rgb *= pixel.a;

  color = clip_scaled (in_pos, pixel * alpha);
}

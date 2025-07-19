#ifndef _COMMON_
#define _COMMON_

void            main_clip_none                  (void);
void            main_clip_rect                  (void);
void            main_clip_rounded               (void);

#include "enums.glsl"

/* Needs to be exactly like this and not use #else
 * because our include script is too dumb
 */
#ifdef VULKAN
#include "common-vulkan.glsl"
#endif /* VULKAN */
#ifndef VULKAN
#include "common-gl.glsl"
#endif

#define GSK_SHADER_CLIP (GSK_FLAGS & 3u)
/* Defined by the shader compilers directly */
/* #define GSK_TEXTURE0_IS_EXTERNAL ((GSK_FLAGS >> 2u) & 1u) */
/* #define GSK_TEXTURE1_IS_EXTERNAL ((GSK_FLAGS >> 3u) & 1u) */
#define GSK_TEXTURE0_SAMPLE_OP ((GSK_FLAGS >> 4u) & 7u)
#define GSK_TEXTURE1_SAMPLE_OP ((GSK_FLAGS >> 8u) & 7u)

#include "color.glsl"
#include "rect.glsl"
#include "roundedrect.glsl"

#define PI 3.1415926535897932384626433832795
#define SQRT1_2 1.4142135623730951

Rect
rect_clip (Rect r)
{
  if (GSK_SHADER_CLIP == GSK_GPU_SHADER_CLIP_NONE)
    return r;
  else
    return rect_intersect (r, rect_from_gsk (GSK_GLOBAL_CLIP_RECT));
}

#ifdef GSK_VERTEX_SHADER

const vec2 offsets[6] = vec2[6](vec2(0.0, 0.0),
                                vec2(1.0, 0.0),
                                vec2(0.0, 1.0),
                                vec2(0.0, 1.0),
                                vec2(1.0, 0.0),
                                vec2(1.0, 1.0));

void
gsk_set_position (vec2 pos)
{
  gl_Position = GSK_GLOBAL_MVP * vec4 (pos, 0.0, 1.0);
}

vec2
rect_get_position (Rect rect)
{
  Rect r = rect_round_larger (rect_clip (rect));

  vec2 pos = mix (r.bounds.xy, r.bounds.zw, offsets[GSK_VERTEX_INDEX]);

  return pos;
}

vec2
border_get_position (RoundedRect outside,
                     RoundedRect inside)
{
  uint slice_index = uint (GSK_VERTEX_INDEX) / 6u;
  uint vert_index = uint (GSK_VERTEX_INDEX) % 6u;

  Rect rect = rounded_rect_intersection_slice (outside, inside, slice_index);

  switch (slice_index)
    {
    case SLICE_TOP_LEFT:
      rect = rect_round_larger (rect);
      rect.bounds = rect.bounds.xwzy;
      break;
    case SLICE_TOP:
      rect = rect_round_smaller_larger (rect);
      break;
    case SLICE_TOP_RIGHT:
      rect = rect_round_larger (rect);
      break;
    case SLICE_RIGHT:
      rect = rect_round_larger_smaller (rect);
      break;
    case SLICE_BOTTOM_RIGHT:
      rect = rect_round_larger (rect);
      rect.bounds = rect.bounds.zyxw;
      break;
    case SLICE_BOTTOM:
      rect = rect_round_smaller_larger (rect);
      break;
    case SLICE_BOTTOM_LEFT:
      rect = rect_round_larger (rect);
      rect.bounds = rect.bounds.zwxy;
      break;
    case SLICE_LEFT:
      rect = rect_round_larger_smaller (rect);
      break;
    }

  vec2 pos = mix (rect.bounds.xy, rect.bounds.zw, offsets[vert_index]);

  return pos;
}

vec2
scale_tex_coord (vec2 in_pos,
                 Rect in_rect,
                 vec4 tex_rect)
{
  return tex_rect.xy + (in_pos - in_rect.bounds.xy) / rect_size (in_rect) * tex_rect.zw;
}

void            run                             (out vec2 pos);

void
main (void)
{
  vec2 pos;

  run (pos);

  gsk_set_position (pos);
}

#endif /* GSK_VERTEX_SHADER */


#ifdef GSK_FRAGMENT_SHADER

vec4
gsk_texture_straight_alpha (sampler2D tex,
                            vec2      pos)
{
  vec2 size = vec2 (textureSize (tex, 0));
  pos *= size;
  size -= vec2 (1.0);
  /* GL_CLAMP_TO_EDGE */
  pos = clamp (pos - 0.5, vec2 (0.0), size);
  ivec2 ipos = ivec2 (pos);
  pos = fract (pos);
  vec4 tl = texelFetch (tex, ipos, 0);
  tl.rgb *= tl.a;
  vec4 tr = texelFetch (tex, ipos + ivec2(1, 0), 0);
  tr.rgb *= tr.a;
  vec4 bl = texelFetch (tex, ipos + ivec2(0, 1), 0);
  bl.rgb *= bl.a;
  vec4 br = texelFetch (tex, ipos + ivec2(1, 1), 0);
  br.rgb *= br.a;
  return mix (mix (tl, tr, pos.x), mix (bl, br, pos.x), pos.y);
}

#if GSK_N_TEXTURES > 0
vec4
gsk_texture0 (vec2 pos)
{
  if (GSK_TEXTURE0_SAMPLE_OP == GDK_SHADER_DEFAULT)
    return texture (GSK_TEXTURE0, pos);
  else if (GSK_TEXTURE0_SAMPLE_OP == GDK_SHADER_STRAIGHT)
    return gsk_texture_straight_alpha (GSK_TEXTURE0, pos);
  else if (GSK_TEXTURE0_SAMPLE_OP == GDK_SHADER_2_PLANES)
    return vec4 (texture (GSK_TEXTURE0, pos).x, texture (GSK_TEXTURE0_1, pos).xy, 1.0).brga;
  else if (GSK_TEXTURE0_SAMPLE_OP == GDK_SHADER_3_PLANES)
    return vec4 (texture (GSK_TEXTURE0_2, pos).x, texture (GSK_TEXTURE0, pos).x, texture (GSK_TEXTURE0_1, pos).x, 1.0);
}
#endif

#if GSK_N_TEXTURES > 1
vec4
gsk_texture1 (vec2 pos)
{
  if (GSK_TEXTURE1_SAMPLE_OP == GDK_SHADER_DEFAULT)
    return texture (GSK_TEXTURE1, pos);
  else if (GSK_TEXTURE1_SAMPLE_OP == GDK_SHADER_STRAIGHT)
    return gsk_texture_straight_alpha (GSK_TEXTURE1, pos);
  else if (GSK_TEXTURE1_SAMPLE_OP == GDK_SHADER_2_PLANES)
    return vec4 (texture (GSK_TEXTURE1, pos).x, texture (GSK_TEXTURE1_1, pos).xy, 1.0).brga;
  else if (GSK_TEXTURE1_SAMPLE_OP == GDK_SHADER_3_PLANES)
    return vec4 (texture (GSK_TEXTURE1_2, pos).x, texture (GSK_TEXTURE1, pos).x, texture (GSK_TEXTURE1_1, pos).x, 1.0);
}
#endif

void            run                             (out vec4 color,
                                                 out vec2 pos);

void
main_clip_none (void)
{
  vec4 color;
  vec2 pos;

  run (color, pos);

  gsk_set_output_color (color);
}

void
main_clip_rect (void)
{
  vec4 color;
  vec2 pos;

  run (color, pos);

  Rect clip = rect_from_gsk (GSK_GLOBAL_CLIP_RECT);

  float coverage = rect_coverage (clip, pos);
  color *= coverage;

  gsk_set_output_color (color);
}

void
main_clip_rounded (void)
{
  vec4 color;
  vec2 pos;

  run (color, pos);

  RoundedRect clip = rounded_rect_from_gsk (GSK_GLOBAL_CLIP);

  float coverage = rounded_rect_coverage (clip, pos);
  color *= coverage;

  gsk_set_output_color (color);
}

void
main (void)
{
  if (GSK_SHADER_CLIP == GSK_GPU_SHADER_CLIP_NONE)
    main_clip_none ();
  else if (GSK_SHADER_CLIP == GSK_GPU_SHADER_CLIP_RECT)
    main_clip_rect ();
  else if (GSK_SHADER_CLIP == GSK_GPU_SHADER_CLIP_ROUNDED)
    main_clip_rounded ();
}

#endif /* GSK_FRAGMENT_SHADER */

#endif


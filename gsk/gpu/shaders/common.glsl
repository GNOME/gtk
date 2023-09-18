#ifndef _COMMON_
#define _COMMON_

void            main_clip_none                  (void);
void            main_clip_rect                  (void);
void            main_clip_rounded               (void);

#include "enums.glsl"

#ifdef VULKAN
#include "common-vulkan.glsl"
#else
#include "common-gl.glsl"
#endif

#include "color.glsl"
#include "rect.glsl"
#include "roundedrect.glsl"

#define PI 3.1415926535897932384626433832795

Rect
rect_clip (Rect r)
{
  if (GSK_SHADER_CLIP == GSK_GPU_SHADER_CLIP_NONE)
    return r;
  else
    return rect_intersect (r, rect_from_gsk (push.clip_bounds));
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
  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);
}

vec2
rect_get_position (Rect rect)
{
  Rect r = rect_round_larger (rect_clip (rect));

  vec2 pos = mix (r.bounds.xy, r.bounds.zw, offsets[GSK_VERTEX_INDEX]);

  return pos;
}

vec2
border_get_position (RoundedRect outline,
                     vec4        border_widths)
{
  uint slice_index = uint (GSK_VERTEX_INDEX) / 6u;
  uint vert_index = uint (GSK_VERTEX_INDEX) % 6u;

  vec4 corner_widths = max (outline.corner_widths, border_widths.wyyw);
  vec4 corner_heights = max (outline.corner_heights, border_widths.xxzz);

  Rect rect;

  switch (slice_index)
    {
    case SLICE_TOP_LEFT:
      rect = Rect (outline.bounds.xyxy + vec4 (0.0, 0.0, corner_widths[TOP_LEFT], corner_heights[TOP_LEFT]));
      rect = rect_round_larger (rect);
      rect.bounds = rect.bounds.xwzy;
      break;
    case SLICE_TOP:
      rect = Rect (vec4 (outline.bounds.x + corner_widths[TOP_LEFT], outline.bounds.y,
                         outline.bounds.z - corner_widths[TOP_RIGHT], outline.bounds.y + border_widths[TOP]));
      rect = rect_round_smaller_larger (rect);
      break;
    case SLICE_TOP_RIGHT:
      rect = Rect (outline.bounds.zyzy + vec4 (- corner_widths[TOP_RIGHT], 0.0, 0.0, corner_heights[TOP_RIGHT]));
      rect = rect_round_larger (rect);
      break;
    case SLICE_RIGHT:
      rect = Rect (vec4 (outline.bounds.z - border_widths[RIGHT], outline.bounds.y + corner_widths[TOP_RIGHT],
                         outline.bounds.z, outline.bounds.w - corner_widths[BOTTOM_RIGHT]));
      rect = rect_round_larger_smaller (rect);
      break;
    case SLICE_BOTTOM_RIGHT:
      rect = Rect (outline.bounds.zwzw + vec4 (- corner_widths[BOTTOM_RIGHT], - corner_heights[BOTTOM_RIGHT], 0.0, 0.0));
      rect = rect_round_larger (rect);
      rect.bounds = rect.bounds.zyxw;
      break;
    case SLICE_BOTTOM:
      rect = Rect (vec4 (outline.bounds.x + corner_widths[BOTTOM_LEFT], outline.bounds.w - border_widths[BOTTOM],
                         outline.bounds.z - corner_widths[BOTTOM_RIGHT], outline.bounds.w));
      rect = rect_round_smaller_larger (rect);
      break;
    case SLICE_BOTTOM_LEFT:
      rect = Rect (outline.bounds.xwxw + vec4 (0.0, - corner_heights[BOTTOM_LEFT], corner_widths[BOTTOM_LEFT], 0.0));
      rect = rect_round_larger (rect);
      rect.bounds = rect.bounds.zwxy;
      break;
    case SLICE_LEFT:
      rect = Rect (vec4 (outline.bounds.x + border_widths[LEFT], outline.bounds.y + corner_widths[TOP_LEFT],
                         outline.bounds.x, outline.bounds.w - corner_widths[BOTTOM_LEFT]));
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

  Rect clip = rect_from_gsk (push.clip_bounds);

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

  RoundedRect clip = RoundedRect(vec4(push.clip_bounds.xy, push.clip_bounds.xy + push.clip_bounds.zw), push.clip_widths, push.clip_heights);
  clip = rounded_rect_scale (clip, push.scale);

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


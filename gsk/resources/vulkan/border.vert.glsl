#version 420 core

#include "constants.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inCornerWidths;
layout(location = 2) in vec4 inCornerHeights;
layout(location = 3) in vec4 inBorderWidths;
layout(location = 4) in mat4 inBorderColors;

layout(location = 0) out vec2 outPos;
layout(location = 1) out flat vec4 outColor;
layout(location = 2) out flat vec4 outRect;
layout(location = 3) out flat vec4 outCornerWidths;
layout(location = 4) out flat vec4 outCornerHeights;
layout(location = 5) out flat vec4 outBorderWidths;

out gl_PerVertex {
  vec4 gl_Position;
};

vec2 offsets[6] = { vec2(0.0, 0.0),
                    vec2(1.0, 0.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 1.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 0.0) };

#define TOP 0
#define RIGHT 1
#define BOTTOM 2
#define LEFT 3

#define TOP_LEFT 0
#define TOP_RIGHT 1
#define BOTTOM_RIGHT 2
#define BOTTOM_LEFT 3

#define SLICE_TOP_LEFT 0
#define SLICE_TOP 1
#define SLICE_TOP_RIGHT 2
#define SLICE_RIGHT 3
#define SLICE_BOTTOM_RIGHT 4
#define SLICE_BOTTOM 5
#define SLICE_BOTTOM_LEFT 6
#define SLICE_LEFT 7

void main() {
  int slice_index = gl_VertexIndex / 6;
  int vert_index = gl_VertexIndex % 6;

  vec4 corner_widths = max (inCornerWidths, inBorderWidths.wyyw);
  vec4 corner_heights = max (inCornerHeights, inBorderWidths.xxzz);

  vec4 rect;

  switch (slice_index)
    {
    case SLICE_TOP_LEFT:
      rect = vec4(inRect.xy, corner_widths[TOP_LEFT], corner_heights[TOP_LEFT]);
      vert_index = (vert_index + 3) % 6;
      break;
    case SLICE_TOP:
      rect = vec4(inRect.x + corner_widths[TOP_LEFT], inRect.y, inRect.z - corner_widths[TOP_LEFT] - corner_widths[TOP_RIGHT], inBorderWidths[TOP]);
      outColor = inBorderColors[TOP];
      break;
    case SLICE_TOP_RIGHT:
      rect = vec4(inRect.x + inRect.z - corner_widths[TOP_RIGHT], inRect.y, corner_widths[TOP_RIGHT], corner_heights[TOP_RIGHT]);
      break;
    case SLICE_RIGHT:
      rect = vec4(inRect.x + inRect.z - inBorderWidths[RIGHT], inRect.y + corner_heights[TOP_RIGHT], inBorderWidths[RIGHT], inRect.w - corner_heights[TOP_RIGHT] - corner_heights[BOTTOM_RIGHT]);
      outColor = inBorderColors[RIGHT];
      break;
    case SLICE_BOTTOM_RIGHT:
      rect = vec4(inRect.x + inRect.z - corner_widths[BOTTOM_RIGHT], inRect.y + inRect.w - corner_heights[BOTTOM_RIGHT], corner_widths[BOTTOM_RIGHT], corner_heights[BOTTOM_RIGHT]);
      break;
    case SLICE_BOTTOM:
      rect = vec4(inRect.x + corner_widths[BOTTOM_LEFT], inRect.y + inRect.w - inBorderWidths[BOTTOM], inRect.z - corner_widths[BOTTOM_LEFT] - corner_widths[BOTTOM_RIGHT], inBorderWidths[BOTTOM]);
      break;
    case SLICE_BOTTOM_LEFT:
      rect = vec4(inRect.x, inRect.y + inRect.w - corner_heights[BOTTOM_LEFT], corner_widths[BOTTOM_LEFT], corner_heights[BOTTOM_LEFT]);
      vert_index = (vert_index + 3) % 6;
      break;
    case SLICE_LEFT:
      rect = vec4(inRect.x, inRect.y + corner_heights[TOP_LEFT], inBorderWidths[LEFT], inRect.w - corner_heights[TOP_LEFT] - corner_heights[BOTTOM_LEFT]);
      break;
    }

  vec2 pos;
  if ((slice_index % 4) != 0 || (vert_index % 3) != 2)
    pos = rect.xy + rect.zw * offsets[vert_index];
  else
    pos = rect.xy + rect.zw * vec2(1.0 - offsets[vert_index].x, offsets[vert_index].y);
  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);
  outColor = inBorderColors[((gl_VertexIndex / 3 + 15) / 4) % 4];
  outPos = pos;
  outRect = inRect;
  outCornerWidths = inCornerWidths;
  outCornerHeights = inCornerHeights;
  outBorderWidths = inBorderWidths;
}

#version 420 core

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inTexRect;
layout(location = 2) in mat4 inColorMatrix;
layout(location = 6) in vec4 inColorOffset;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 clip_bounds;
    vec4 clip_widths;
    vec4 clip_heights;
} push;

layout(location = 0) out vec2 outPos;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out flat vec4 outClipBounds;
layout(location = 3) out flat vec4 outClipWidths;
layout(location = 4) out flat mat4 outColorMatrix;
layout(location = 8) out flat vec4 outColorOffset;

out gl_PerVertex {
  vec4 gl_Position;
};

vec2 offsets[6] = { vec2(0.0, 0.0),
                    vec2(1.0, 0.0),
                    vec2(0.0, 1.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 0.0),
                    vec2(1.0, 1.0) };

vec4 intersect(vec4 a, vec4 b)
{
  a = vec4(a.xy, a.xy + a.zw);
  b = vec4(b.xy, b.xy + b.zw);
  vec4 result = vec4(max(a.xy, b.xy), min(a.zw, b.zw));
  if (any (greaterThanEqual (result.xy, result.zw)))
    return vec4(0.0,0.0,0.0,0.0);
  return vec4(result.xy, result.zw - result.xy);
}

void main() {
  vec4 rect = intersect(inRect, push.clip_bounds);
  vec2 pos = rect.xy + rect.zw * offsets[gl_VertexIndex];
  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);

  outPos = pos;
  outClipBounds = push.clip_bounds;
  outClipWidths = push.clip_widths;

  vec4 texrect = vec4((rect.xy - inRect.xy) / inRect.zw,
                      rect.zw / inRect.zw);
  texrect = vec4(inTexRect.xy + inTexRect.zw * texrect.xy,
                 inTexRect.zw * texrect.zw);
  outTexCoord = texrect.xy + texrect.zw * offsets[gl_VertexIndex];
  outColorMatrix = inColorMatrix;
  outColorOffset = inColorOffset;
}

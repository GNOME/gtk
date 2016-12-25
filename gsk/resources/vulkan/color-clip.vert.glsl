#version 420 core

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 clip_bounds;
    vec4 clip_widths;
    vec4 clip_heights;
} push;

out gl_PerVertex {
  vec4 gl_Position;
};

layout(location = 0) out vec4 outColor;

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
  outColor = inColor;
}

#version 420 core

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 clip_bounds;
    vec4 clip_widths;
    vec4 clip_heights;
} push;

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inTexRect1;
layout(location = 2) in vec4 inTexRect2;
layout(location = 3) in float inTime;

layout(location = 0) out vec2 outPos;
layout(location = 1) out vec2 outTexCoord1;
layout(location = 2) out vec2 outTexCoord2;
layout(location = 3) out float outTime;
layout(location = 4) out vec2 outResolution;

out gl_PerVertex {
  vec4 gl_Position;
};

vec4 clip(vec4 rect) { return rect; }

vec2 offsets[6] = { vec2(0.0, 0.0),
                    vec2(1.0, 0.0),
                    vec2(0.0, 1.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 0.0),
                    vec2(1.0, 1.0) };

void main() {
  vec4 rect = clip (inRect);
  vec2 pos = rect.xy + rect.zw * offsets[gl_VertexIndex];
  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);

  outPos = pos;

  vec4 texrect = vec4((rect.xy - inRect.xy) / inRect.zw,
                      rect.zw / inRect.zw);
  vec4 texrect1 = vec4(inTexRect1.xy + inTexRect1.zw * texrect.xy,
                      inTexRect1.zw * texrect.zw);
  outTexCoord1 = texrect1.xy + texrect1.zw * offsets[gl_VertexIndex];

  vec4 texrect2 = vec4(inTexRect2.xy + inTexRect2.zw * texrect.xy,
                      inTexRect2.zw * texrect.zw);
  outTexCoord2 = texrect2.xy + texrect2.zw * offsets[gl_VertexIndex];

  outTime = inTime;
  outResolution = inRect.zw;
}
#version 420 core

#include "clip.vert.glsl"

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inStartTexRect;
layout(location = 2) in vec4 inEndTexRect;
layout(location = 3) in uint inBlendMode;

layout(location = 0) out vec2 outPos;
layout(location = 1) out vec2 outStartTexCoord;
layout(location = 2) out vec2 outEndTexCoord;
layout(location = 3) flat out uint outBlendMode;

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
  vec4 starttexrect = vec4(inStartTexRect.xy + inStartTexRect.zw * texrect.xy,
                           inStartTexRect.zw * texrect.zw);
  vec4 endtexrect = vec4(inEndTexRect.xy + inEndTexRect.zw * texrect.xy,
                         inEndTexRect.zw * texrect.zw);

  outStartTexCoord = starttexrect.xy + starttexrect.zw * offsets[gl_VertexIndex];
  outEndTexCoord = endtexrect.xy + endtexrect.zw * offsets[gl_VertexIndex];

  outBlendMode = inBlendMode;
}

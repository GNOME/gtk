#version 420 core

#include "clip.vert.glsl"

layout(location = 0) in vec4 inOutline;
layout(location = 1) in vec4 inOutlineCornerWidths;
layout(location = 2) in vec4 inOutlineCornerHeights;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec2 inOffset;
layout(location = 5) in float inSpread;
layout(location = 6) in float inBlurRadius;

layout(location = 0) out vec2 outPos;
layout(location = 1) out flat vec4 outOutline;
layout(location = 2) out flat vec4 outOutlineCornerWidths;
layout(location = 3) out flat vec4 outOutlineCornerHeights;
layout(location = 4) out flat vec4 outColor;
layout(location = 5) out flat vec2 outOffset;
layout(location = 6) out flat float outSpread;
layout(location = 7) out flat float outBlurRadius;

vec2 offsets[6] = { vec2(0.0, 0.0),
                    vec2(1.0, 0.0),
                    vec2(0.0, 1.0),
                    vec2(0.0, 1.0),
                    vec2(1.0, 0.0),
                    vec2(1.0, 1.0) };

float radius_pixels(float radius) {
  return radius * (3.0 * sqrt(2 * 3.141592653589793) / 4) * 1.5;
}

void main() {
  vec4 rect = inOutline;
  float spread = inSpread + radius_pixels(inBlurRadius);
  rect += vec4(inOffset - spread, vec2(2 * spread));
  
  clip (inOutline);

  vec2 pos = rect.xy + rect.zw * offsets[gl_VertexIndex];
  gl_Position = push.mvp * vec4 (pos, 0.0, 1.0);
  outPos = pos;

  outOutline = inOutline;
  outOutlineCornerWidths = inOutlineCornerWidths;
  outOutlineCornerHeights = inOutlineCornerHeights;
  outColor = inColor;
  outOffset = inOffset;
  outSpread = inSpread;
  outBlurRadius = inBlurRadius;
}

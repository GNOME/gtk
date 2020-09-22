uniform float progress;

vec4 getFromColor(vec2 uv) {
  return texture(u_source, uv);
}

vec4 getToColor(vec2 uv) {
  return texture(u_source2, uv);
}


// Source: https://gl-transitions.com/editor/wind
// Author: gre
// License: MIT

uniform float size = 0.2;

float rand(vec2 co) {
  return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec4 transition(vec2 p) {
  float r = rand(vec2(0, p.y));
  float m = smoothstep(0.0, -size, p.x*(1.0-size) + size*r - (progress * (1.0 + size)));
  return mix(getFromColor(p), getToColor(p), m);
}


void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  fragColor = transition(uv);
}

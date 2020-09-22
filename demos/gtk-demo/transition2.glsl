uniform float progress;

vec4 getFromColor (vec2 uv) {
  return texture(u_source, uv);
}

vec4 getToColor (vec2 uv) {
  return texture(u_source2, uv);
}


// Source: https://gl-transitions.com/editor/Radial
// License: MIT
// Author: Xaychru

uniform float smoothness = 1.0;

const float PI = 3.141592653589;

vec4 transition(vec2 p) {
  vec2 rp = p*2.-1.;
  return mix(
    getToColor(p),
    getFromColor(p),
    smoothstep(0., smoothness, atan(rp.y,rp.x) - (progress-.5) * PI * 2.5)
  );
}


void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  fragColor = transition(uv);
}

float progress = u_args.y;

vec4 getFromColor (vec2 uv) {
  return texture(u_source, uv);
}

vec4 getToColor (vec2 uv) {
  return texture(u_source2, uv);
}


// Source: https://gl-transitions.com/editor/crosswarp
// Author: Eke Péter <peterekepeter@gmail.com>
// License: MIT

vec4 transition(vec2 p) {
  float x = progress;
  x=smoothstep(.0,1.0,(x*2.0+p.x-1.0));
  return mix(getFromColor((p-.5)*(1.-x)+.5), getToColor((p-.5)*x+.5), x);
}


void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  fragColor = transition(uv);
}

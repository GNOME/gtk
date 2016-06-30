#version 150

smooth in vec2 vUv;

out vec4 outputColor;

uniform mat4 mvp;
uniform sampler2D map;
uniform sampler2D parentMap;
uniform float alpha;
uniform int blendMode;

vec3 BlendMultiply(vec3 Csrc, vec3 Cdst) {
  return Csrc * Cdst;
}


void main() {
  vec4 src = texture2D(map, vUv);
  vec4 dst = texture2D(parentMap, vUv);
  vec3 res;

  if (blendMode == 0) {
    res = src.xyz;
  }
  else if (blendMode == 1) {
    res = BlendMultiply(src.xyz, dst.xyz);
  }
  else {
    res = vec3(1.0, 1.0, 0.0);
  }

  outputColor = vec4(res, src.a * alpha);
}

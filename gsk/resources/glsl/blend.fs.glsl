vec3 BlendMultiply(vec3 Csrc, vec3 Cdst) {
  return Csrc * Cdst;
}

void main() {
  vec4 src = Texture(map, vUv);
  vec4 mask = Texture(parentMap, vUv);
  vec3 res;

  if (blendMode == 0) {
    res = src.xyz;
  }
  else if (blendMode == 1) {
    res = BlendMultiply(src.xyz, mask.xyz);
  }
  else {
    // Use red for debugging missing blend modes
    res = vec3(1.0, 0.0, 0.0);
  }

  setOutputColor(vec4(res, src.a * alpha));
}

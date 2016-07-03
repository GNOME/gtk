vec3 BlendMultiply(vec3 Cb, vec3 Cs) {
  return Cb * Cs;
}

vec3 BlendScreen(vec3 Cb, vec3 Cs) {
  return Cb + Cs - (Cb * Cs);
}

vec3 BlendHardLight(vec3 Cb, vec3 Cs) {
  vec3 m = BlendMultiply(Cb, 2.0 * Cs);
  vec3 s = BlendScreen(Cb, 2.0 * Cs - 1.0);
  vec3 edge = vec3(0.5, 0.5, 0.5);

  /* Use mix() and step() to avoid a branch */
  return mix(m, s, step(edge, Cs));
}

vec3 BlendOverlay(vec3 Cb, vec3 Cs) {
  return BlendHardLight(Cs, Cb);
}

vec3 BlendDarken(vec3 Cb, vec3 Cs) {
  return min(Cb, Cs);
}

vec3 BlendLighten(vec3 Cb, vec3 Cs) {
  return max(Cb, Cs);
}

void main() {
  vec4 mask = Texture(parentMap, vUv);
  vec4 diffuse = Texture(map, vUv);
  vec3 res;

  if (blendMode == 0) {
    res = diffuse.xyz;
  }
  else if (blendMode == 1) {
    res = BlendMultiply(mask.xyz, diffuse.xyz);
  }
  else if (blendMode == 2) {
    res = BlendScreen(mask.xyz, diffuse.xyz);
  }
  else if (blendMode == 3) {
    res = BlendOverlay(mask.xyz, diffuse.xyz);
  }
  else if (blendMode == 4) {
    res = BlendDarken(mask.xyz, diffuse.xyz);
  }
  else if (blendMode == 5) {
    res = BlendLighten(mask.xyz, diffuse.xyz);
  }
  else if (blendMode == 8) {
    res = BlendHardLight(mask.xyz, diffuse.xyz);
  }
  else {
    // Use red for debugging missing blend modes
    res = vec3(1.0, 0.0, 0.0);
  }

  setOutputColor(vec4(res, diffuse.a * alpha));
}

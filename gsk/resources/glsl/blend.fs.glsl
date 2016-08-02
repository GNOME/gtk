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
  vec4 Cs = Texture(uSource, vUv);
  vec4 Cb = Texture(uMask, vUv);
  vec3 res;

  if (uBlendMode == 0) {
    res = Cs.xyz;
  }
  else if (uBlendMode == 1) {
    res = BlendMultiply(Cb.xyz, Cs.xyz);
  }
  else if (uBlendMode == 2) {
    res = BlendScreen(Cb.xyz, Cs.xyz);
  }
  else if (uBlendMode == 3) {
    res = BlendOverlay(Cb.xyz, Cs.xyz);
  }
  else if (uBlendMode == 4) {
    res = BlendDarken(Cb.xyz, Cs.xyz);
  }
  else if (uBlendMode == 5) {
    res = BlendLighten(Cb.xyz, Cs.xyz);
  }
  else if (uBlendMode == 8) {
    res = BlendHardLight(Cb.xyz, Cs.xyz);
  }
  else {
    // Use red for debugging missing blend modes
    res = vec3(1.0, 0.0, 0.0);
  }

  setOutputColor(vec4(res, Cs.a * uAlpha));
}

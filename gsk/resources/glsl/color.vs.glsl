void main() {
  gl_Position = uMVP * vec4(aPosition, 0.0, 1.0);

  // Flip the sampling
  vUv = vec2(aUv.x, aUv.y);
}

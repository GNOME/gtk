void main() {
  gl_Position = mvp * vec4(position, 0.0, 1.0);

  // Flip the sampling
  vUv = vec2(uv.x, 1.0 - uv.y);
}

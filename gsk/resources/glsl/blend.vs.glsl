void main() {
  gl_Position = uModelview * uProjection * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

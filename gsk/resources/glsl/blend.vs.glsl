void main() {
  gl_Position = u_modelview * u_projection * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

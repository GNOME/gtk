// VERTEX_SHADER:
void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
void main() {
  vec4 diffuse = Texture(u_source, vUv);

  setOutputColor(diffuse * u_alpha);
}


// VERTEX_SHADER:
// external.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// external.glsl

#ifdef GSK_GLES
uniform samplerExternalOES u_external_source;
#else
uniform sampler2D u_external_source;
#endif

void main() {
  vec4 diffuse = texture2D(u_external_source, vUv);

  gskSetOutputColor(diffuse);
}

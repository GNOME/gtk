// VERTEX_SHADER:
// dmabuf.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// dmabuf.glsl

#extension GL_OES_EGL_image_external : require

uniform samplerExternalOES u_source_dmabuf;

void main() {
  vec4 rgb = GskTexture(u_source_dmabuf, vUv);

  gskSetOutputColor(rgb);
}

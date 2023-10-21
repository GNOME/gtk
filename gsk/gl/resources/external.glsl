// VERTEX_SHADER:
// external.glsl

#version 300 es

precision highp float;

in vec2 aPosition;
in vec2 aUv;
out vec2 vUv;
uniform mat4 u_projection;
uniform mat4 u_modelview;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// external.glsl

#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require

precision highp float;

uniform samplerExternalOES u_external_source;

in vec2 vUv;

out vec4 outputColor;

void main() {
  vec4 color = texture(u_external_source, vUv);
  outputColor = color;
}

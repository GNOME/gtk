// VERTEX_SHADER:
// external.glsl

#version 100

precision highp float;

attribute vec2 aPosition;
attribute vec2 aUv;
varying vec2 vUv;
uniform mat4 u_projection;
uniform mat4 u_modelview;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// external.glsl

#version 100
#extension GL_OES_EGL_image_external : require

precision highp float;

uniform samplerExternalOES u_external_source;

varying vec2 vUv;

void main() {
  vec4 color = texture2D(u_external_source, vUv);
  gl_FragColor = color;
}

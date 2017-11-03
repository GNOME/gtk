precision highp float;

uniform sampler2D uSource;
uniform sampler2D uMask;
uniform mat4 uMVP;
uniform float uAlpha = 1.0;
uniform int uBlendMode;
uniform vec4 uViewport;

in vec2 vUv;

out vec4 outputColor;

vec4 Texture(sampler2D sampler, vec2 texCoords) {
  return texture(sampler, texCoords);
}

void setOutputColor(vec4 color) {
  outputColor = color;
}

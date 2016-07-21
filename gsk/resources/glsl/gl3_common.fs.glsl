precision highp float;

uniform sampler2D uSource;
uniform sampler2D uMask;
uniform mat4 uMVP;
uniform float uAlpha;
uniform int uBlendMode;

in vec2 vUv;

out vec4 outputColor;

vec4 Texture(sampler2D sampler, vec2 texCoords) {
  return texture(sampler, texCoords);
}

void setOutputColor(vec4 color) {
  outputColor = color;
}

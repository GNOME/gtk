uniform mat4 uMVP;
uniform sampler2D uSource;
uniform sampler2D uMask;
uniform float uAlpha;
uniform int uBlendMode;

varying vec2 vUv;

vec4 Texture(sampler2D sampler, vec2 texCoords) {
  return texture2D(sampler, texCoords);
}

void setOutputColor(vec4 color) {
  gl_FragColor = color;
}

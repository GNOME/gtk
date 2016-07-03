precision highp float;

uniform sampler2D map;
uniform sampler2D parentMap;
uniform mat4 mvp;
uniform float alpha;
uniform int blendMode;

in vec2 vUv;

out vec4 outputColor;

vec4 Texture(sampler2D sampler, vec2 texCoords) {
  return texture(sampler, texCoords);
}

void setOutputColor(vec4 color) {
  outputColor = color;
}

precision mediump float;

uniform mat4 mvp;
uniform sampler2D map;
uniform sampler2D parentMap;
uniform float alpha;

varying vec2 vUv;

vec4 Texture(sampler2D sampler, vec2 texCoords) {
  return texture2D(sampler, texCoords);
}

void setOutputColor(vec4 color) {
  gl_FragColor = color;
}

// VERTEX_SHADER:
void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);
  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// The shader supplies:
void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv);

uniform vec2 u_size;
uniform vec4 u_args;
uniform sampler2D u_source2;

void main() {
  vec4 fragColor;
  vec2 fragCoord = vec2(vUv.x * u_size.x, (1.0-vUv.y) * u_size.y);
  mainImage(fragColor, fragCoord, u_size, vUv);
  gskSetOutputColor(fragColor);
}

attribute vec2 position;
attribute vec2 uv;

varying highp vec2 vUv;

void main() {
  vUv = uv;

  gl_Position = vec4(position, 0.0, 1.0);
}

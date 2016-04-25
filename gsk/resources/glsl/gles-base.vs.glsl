uniform mat4 mvp;

attribute vec2 position;
attribute vec2 uv;

varying vec2 vUv;

void main() {
  gl_Position = mvp * vec4(position, 0.0, 1.0);

  vUv = vec2(uv.x, 1.0 - uv.y);
}

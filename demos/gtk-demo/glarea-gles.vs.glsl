attribute vec4 in_position;
attribute vec4 in_color;

uniform mat4 mvp;

varying vec4 color;

void main() {
  color = in_color;
  gl_Position = mvp * in_position;
}

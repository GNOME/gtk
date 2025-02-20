#version 330

in vec4 in_position;
in vec4 in_color;
uniform mat4 mvp;

out vec4 color;

void main() {
  color = in_color;
  gl_Position = mvp * in_position;
}

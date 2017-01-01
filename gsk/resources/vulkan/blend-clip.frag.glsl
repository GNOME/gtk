#version 420 core

layout(location = 0) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform sampler2D inTexture;

layout(location = 0) out vec4 color;

void main()
{
  color = texture (inTexture, inTexCoord);
}

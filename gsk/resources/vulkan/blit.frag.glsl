#version 420 core

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 color;

void main()
{
    color = vec4 (inTexCoord, 0.0, 1.0);
}

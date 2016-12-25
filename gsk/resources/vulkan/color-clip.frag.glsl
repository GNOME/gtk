#version 420 core

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 color;

void main()
{
    color = vec4(inColor.rgb * inColor.a, inColor.a);
}

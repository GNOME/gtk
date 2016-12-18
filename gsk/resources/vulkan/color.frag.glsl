#version 420 core

layout(location = 0) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 color;
} push;

layout(location = 0) out vec4 color;

void main()
{
    color = inColor;
}

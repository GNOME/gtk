#version 420 core

layout(location = 0) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform sampler2D inTexture;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 color;
} push;

layout(location = 0) out vec4 color;

void main()
{
    color = push.color;
}

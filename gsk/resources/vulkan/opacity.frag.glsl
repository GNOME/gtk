#version 420 core

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in flat float inOpacity;

layout(set = 0, binding = 0) uniform sampler2D inTexture;

layout(location = 0) out vec4 color;

vec4
opacity (vec4 color, float value)
{
  return color * value;
}

void main()
{
  color = opacity (texture (inTexture, inTexCoord), inOpacity);
}

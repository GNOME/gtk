#version 420 core

#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in float inRadius;

layout(set = 0, binding = 0) uniform sampler2D inTexture;

layout(location = 0) out vec4 color;

const int   c_samplesX    = 15;  // must be odd
const int   c_samplesY    = 15;  // must be odd
const float c_textureSize = 512.0;

const int   c_halfSamplesX = c_samplesX / 2;
const int   c_halfSamplesY = c_samplesY / 2;
const float c_pixelSize = (1.0 / c_textureSize);

float Gaussian (float sigma, float x)
{
    return exp(-(x*x) / (2.0 * sigma*sigma));
}

vec3 BlurredPixel (in vec2 uv)
{
    float c_sigmaX      = inRadius;
    float c_sigmaY      = inRadius;
    float total = 0.0;
    vec3 ret = vec3(0);

    for (int iy = 0; iy < c_samplesY; ++iy)
    {
        float fy = Gaussian (c_sigmaY, float(iy) - float(c_halfSamplesY));
        float offsety = float(iy-c_halfSamplesY) * c_pixelSize;
        for (int ix = 0; ix < c_samplesX; ++ix)
        {
            float fx = Gaussian (c_sigmaX, float(ix) - float(c_halfSamplesX));
            float offsetx = float(ix-c_halfSamplesX) * c_pixelSize;
            total += fx * fy;
            ret += texture(inTexture, uv + vec2(offsetx, offsety)).rgb * fx*fy;
        }
    }
    return ret / total;
}

void main()
{
//  color = clip (inPos, vec4(BlurredPixel (inTexCoord), 1.0));
  color = clip (inPos, vec4(1.0, 0.0, 0.0, 1.0));
}

#version 420 core

#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inStartTexCoord;
layout(location = 2) in vec2 inEndTexCoord;
layout(location = 3) flat in uint inBlendMode;

layout(set = 0, binding = 0) uniform sampler2D startTexture;
layout(set = 1, binding = 0) uniform sampler2D endTexture;

layout(location = 0) out vec4 outColor;

float
combine (float source, float backdrop)
{
  return source + backdrop * (1 - source);
}

vec4
composite (vec4 Cs, vec4 Cb, vec3 B)
{
  float ao = Cs.a + Cb.a * (1 - Cs.a);
  vec3 Co = (Cs.a*(1 - Cb.a)*Cs.rgb + Cs.a*Cb.a*B + (1 - Cs.a)*Cb.a*Cb.rgb) / ao;
  return vec4(Co, ao);
}

vec4
normal (vec4 Cs, vec4 Cb)
{
  return composite (Cs, Cb, Cs.rgb);
}

vec4
multiply (vec4 Cs, vec4 Cb)
{
  return composite (Cs, Cb, Cs.rgb * Cb.rgb);
}

vec4
difference (vec4 Cs, vec4 Cb)
{
  return composite (Cs, Cb, abs(Cs.rgb - Cb.rgb));
}

vec4
screen (vec4 Cs, vec4 Cb)
{
  return composite (Cs, Cb, Cs.rgb + Cb.rgb - Cs.rgb * Cb.rgb);
}

float
hard_light (float source, float backdrop)
{
  if (source <= 0.5)
    return 2 * backdrop * source;
  else
    return 2 * (backdrop + source - backdrop * source) - 1;
}

vec4
hard_light (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (hard_light (Cs.r, Cb.r),
                 hard_light (Cs.g, Cb.g),
                 hard_light (Cs.b, Cb.b));
  return composite (Cs, Cb, B);
}

float
soft_light (float source, float backdrop)
{
  float db;

  if (backdrop <= 0.25)
    db = ((16 * backdrop - 12) * backdrop + 4) * backdrop;
  else
    db = sqrt (backdrop);

  if (source <= 0.5)
    return backdrop - (1 - 2 * source) * backdrop * (1 - backdrop);
  else
    return backdrop + (2 * source - 1) * (db - backdrop);
}

vec4
soft_light (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (soft_light (Cs.r, Cb.r),
                 soft_light (Cs.g, Cb.g),
                 soft_light (Cs.b, Cb.b));
  return composite (Cs, Cb, B);
}

vec4
overlay (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (hard_light (Cb.r, Cs.r),
                 hard_light (Cb.g, Cs.g),
                 hard_light (Cb.b, Cs.b));
  return composite (Cs, Cb, B);
}

vec4
darken (vec4 Cs, vec4 Cb)
{
  vec3 B = min (Cs.rgb, Cb.rgb);
  return composite (Cs, Cb, B);
}

vec4
lighten (vec4 Cs, vec4 Cb)
{
  vec3 B = max (Cs.rgb, Cb.rgb);
  return composite (Cs, Cb, B);
}

float
color_dodge (float source, float backdrop)
{
  return (source == 1.0) ? source : min (backdrop / (1.0 - source), 1.0);
}

vec4
color_dodge (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (color_dodge (Cs.r, Cb.r),
                 color_dodge (Cs.g, Cb.g),
                 color_dodge (Cs.b, Cb.b));
  return composite (Cs, Cb, B);
}


float
color_burn (float source, float backdrop)
{
  return (source == 0.0) ? source : max ((1.0 - ((1.0 - backdrop) / source)), 0.0);
}

vec4
color_burn (vec4 Cs, vec4 Cb)
{
  vec3 B = vec3 (color_burn (Cs.r, Cb.r),
                 color_burn (Cs.g, Cb.g),
                 color_burn (Cs.b, Cb.b));
  return composite (Cs, Cb, B);
}

vec4
exclusion (vec4 Cs, vec4 Cb)
{
  vec3 B = Cb.rgb + Cs.rgb - 2.0 * Cb.rgb * Cs.rgb;
  return composite (Cs, Cb, B);
}

float
lum (vec3 c)
{
  return 0.3 * c.r + 0.59 * c.g + 0.11 * c.b;
}

vec3
clip_color (vec3 c)
{
  float l = lum (c);
  float n = min (c.r, min (c.g, c.b));
  float x = max (c.r, max (c.g, c.b));
  if (n < 0) c = l + (((c - l) * l) / (l - n));
  if (x > 1) c = l + (((c - l) * (1 - l)) / (x - l));
  return c;
}

vec3
set_lum (vec3 c, float l)
{
  float d = l - lum (c);
  return clip_color (vec3 (c.r + d, c.g + d, c.b + d));
}

float
sat (vec3 c)
{
  return max (c.r, max (c.g, c.b)) - min (c.r, min (c.g, c.b));
}

vec3
set_sat (vec3 c, float s)
{
  float cmin = min (c.r, min (c.g, c.b));
  float cmax = max (c.r, max (c.g, c.b));
  vec3 res;

  if (cmax == cmin)
    res = vec3 (0, 0, 0);
  else
    {
      if (c.r == cmax)
        {
          if (c.g == cmin)
            {
              res.b = ((c.b - cmin) * s) / (cmax - cmin);
              res.g = 0;
            }
          else
            {
              res.g = ((c.g - cmin) * s) / (cmax - cmin);
              res.b = 0;
            }
          res.r = s;
        }
      else if (c.g == cmax)
        {
          if (c.r == cmin)
            {
              res.b = ((c.b - cmin) * s) / (cmax - cmin);
              res.r = 0;
            }
          else
            {
              res.r = ((c.r - cmin) * s) / (cmax - cmin);
              res.b = 0;
            }
          res.g = s;
        }
      else
        {
          if (c.r == cmin)
            {
              res.g = ((c.g - cmin) * s) / (cmax - cmin);
              res.r = 0;
            }
          else
            {
              res.r = ((c.r - cmin) * s) / (cmax - cmin);
              res.g = 0;
            }
          res.b = s;
        }
    }
  return res;
}

vec4
color (vec4 Cs, vec4 Cb)
{
  vec3 B = set_lum (Cs.rgb, lum (Cb.rgb));
  return composite (Cs, Cb, B);
}

vec4
hue (vec4 Cs, vec4 Cb)
{
  vec3 B = set_lum (set_sat (Cs.rgb, sat (Cb.rgb)), lum (Cb.rgb));
  return composite (Cs, Cb, B);
}

vec4
saturation (vec4 Cs, vec4 Cb)
{
  vec3 B = set_lum (set_sat (Cb.rgb, sat (Cs.rgb)), lum (Cb.rgb));
  return composite (Cs, Cb, B);
}

vec4
luminosity (vec4 Cs, vec4 Cb)
{
  vec3 B = set_lum (Cb.rgb, lum (Cs.rgb));
  return composite (Cs, Cb, B);
}

void main()
{
  vec4 source = texture (startTexture, inStartTexCoord);
  vec4 backdrop = texture (endTexture, inEndTexCoord);
  vec4 result;

  if (inBlendMode == 0)
    result = normal (source, backdrop);
  else if (inBlendMode == 1)
    result = multiply (source, backdrop);
  else if (inBlendMode == 2)
    result = screen (source, backdrop);
  else if (inBlendMode == 3)
    result = overlay (source, backdrop);
  else if (inBlendMode == 4)
    result = darken (source, backdrop);
  else if (inBlendMode == 5)
    result = lighten (source, backdrop);
  else if (inBlendMode == 6)
    result = color_dodge (source, backdrop);
  else if (inBlendMode == 7)
    result = color_burn (source, backdrop);
  else if (inBlendMode == 8)
    result = hard_light (source, backdrop);
  else if (inBlendMode == 9)
    result = soft_light (source, backdrop);
  else if (inBlendMode == 10)
    result = difference (source, backdrop);
  else if (inBlendMode == 11)
    result = exclusion (source, backdrop);
  else if (inBlendMode == 12)
    result = color (source, backdrop);
  else if (inBlendMode == 13)
    result = hue (source, backdrop);
  else if (inBlendMode == 14)
    result = saturation (source, backdrop);
  else if (inBlendMode == 15)
    result = luminosity (source, backdrop);
  else
    discard;

  outColor = clip (inPos, result);
}

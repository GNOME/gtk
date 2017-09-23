#version 420 core

#include "clip.frag.glsl"

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inStartTexCoord;
layout(location = 2) in vec2 inEndTexCoord;
layout(location = 3) flat in uint inBlendMode;

layout(set = 0, binding = 0) uniform sampler2D startTexture;
layout(set = 1, binding = 0) uniform sampler2D endTexture;

layout(location = 0) out vec4 outColor;

vec3
multiply (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = source * backdrop;
  return mix (source, result, opacity);
}

vec3
difference (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = abs (source - backdrop);
  return mix (source, result, opacity);
}

vec3
screen (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = source + backdrop - source * backdrop;
  return mix (source, result, opacity);
}

float
hard_light (float source, float backdrop)
{
  if (source <= 0.5)
    return 2 * backdrop * source;
  else
    return 2 * (backdrop + source - backdrop * source) - 1;
}

vec3
hard_light (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = vec3 (hard_light (source.r, backdrop.r),
                      hard_light (source.g, backdrop.g),
                      hard_light (source.b, backdrop.b));
  return mix (source, result, opacity);
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

vec3
soft_light (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = vec3 (soft_light (source.r, backdrop.r),
                      soft_light (source.g, backdrop.g),
                      soft_light (source.b, backdrop.b));
  return mix (source, result, opacity);
}

vec3
overlay (vec3 source, vec3 backdrop, float opacity)
{
  return hard_light (backdrop, source, opacity);
}

vec3
darken (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = min (source, backdrop);
  return mix (source, result, opacity);
}

vec3
lighten (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = max (source, backdrop);
  return mix (source, result, opacity);
}

float
color_dodge (float source, float backdrop)
{
  return (source == 1.0) ? source : min (backdrop / (1.0 - source), 1.0);
}

vec3
color_dodge (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = vec3 (color_dodge (source.r, backdrop.r),
                      color_dodge (source.g, backdrop.g),
                      color_dodge (source.b, backdrop.b));
  return mix (source, result, opacity);
}


float
color_burn (float source, float backdrop)
{
  return (source == 0.0) ? source : max ((1.0 - ((1.0 - backdrop) / source)), 0.0);
}

vec3
color_burn (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = vec3 (color_burn (source.r, backdrop.r),
                      color_burn (source.g, backdrop.g),
                      color_burn (source.b, backdrop.b));
  return mix (source, result, opacity);
}

vec3
exclusion (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = backdrop + source - 2.0 * backdrop * source;
  return mix (source, result, opacity);
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

vec3
color (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = set_lum (source, lum (backdrop));
  return mix (source, result, opacity);
}

vec3
hue (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = set_lum (set_sat (source, sat (backdrop)), lum (backdrop));
  return mix (source, result, opacity);
}

vec3
saturation (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = set_lum (set_sat (backdrop, sat (source)), lum (backdrop));
  return mix (source, result, opacity);
}

vec3
luminosity (vec3 source, vec3 backdrop, float opacity)
{
  vec3 result = set_lum (backdrop, lum (source));
  return mix (source, result, opacity);
}

float
combine (float source, float backdrop)
{
  return source + backdrop * (1 - source);
}

void main()
{
  vec4 source = texture (startTexture, inStartTexCoord);
  vec4 backdrop = texture (endTexture, inEndTexCoord);
  vec3 rgb = vec3(1,0,0);
  float a = 1;

  a = combine (source.a, backdrop.a);

  if (inBlendMode == 0) // default
    {
      rgb = source.rgb;
      a = source.a;
    }
  else if (inBlendMode == 1)
    rgb = multiply (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 2)
    rgb = screen (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 3)
    rgb = overlay (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 4)
    rgb = darken (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 5)
    rgb = lighten (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 6)
    rgb = color_dodge (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 7)
    rgb = color_burn (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 8)
    rgb = hard_light (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 9)
    rgb = soft_light (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 10)
    rgb = difference (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 11)
    rgb = exclusion (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 12)
    rgb = color (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 13)
    rgb = hue (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 14)
    rgb = saturation (source.rgb, backdrop.rgb, backdrop.a);
  else if (inBlendMode == 15)
    rgb = luminosity (source.rgb, backdrop.rgb, backdrop.a);
  else
    discard;

  outColor = clip (inPos, vec4 (rgb, a));
}

#ifndef _COLOR_
#define _COLOR_

vec4
color_premultiply (vec4 color)
{
  return vec4 (color.rgb, 1.0) * color.a;
}

vec4
color_unpremultiply (vec4 color)
{
  return color.a > 0.0 ? color / vec4 (color.aaa, 1.0) : color;
}

float
luminance (vec3 color)
{
  return dot (vec3 (0.2126, 0.7152, 0.0722), color);
}

float
apply_gamma (float v)
{
  if (v > 0.0031308)
    return 1.055 * pow (v, 1.0/2.4) - 0.055;
  else
    return 12.92 * v;
}

float
unapply_gamma (float v)
{
  if (v >= 0.04045)
    return pow (((v + 0.055)/(1.0 + 0.055)), 2.4);
  else
    return v / 12.92;
}

vec4
srgb_to_linear (vec4 c)
{
  return vec4(unapply_gamma (c.r),
              unapply_gamma (c.g),
              unapply_gamma (c.b),
              c.a);
}

vec4
srgb_from_linear (vec4 c)
{
  return vec4(apply_gamma (c.r),
              apply_gamma (c.g),
              apply_gamma (c.b),
              c.a);
}

#endif /* _COLOR_ */


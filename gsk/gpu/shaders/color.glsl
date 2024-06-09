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
srgb_transfer_function (float v)
{
  if (v >= 0.04045)
    return pow (((v + 0.055)/(1.0 + 0.055)), 2.4);
  else
    return v / 12.92;
}

float
srgb_inverse_transfer_function (float v)
{
  if (v > 0.0031308)
    return 1.055 * pow (v, 1.0/2.4) - 0.055;
  else
    return 12.92 * v;
}

vec4
srgb_to_linear_srgb (vec4 color)
{
  return vec4 (srgb_transfer_function (color.r),
               srgb_transfer_function (color.g),
               srgb_transfer_function (color.b),
               color.a);
}

vec4
linear_srgb_to_srgb (vec4 color)
{
  return vec4 (srgb_inverse_transfer_function (color.r),
               srgb_inverse_transfer_function (color.g),
               srgb_inverse_transfer_function (color.b),
               color.a);
}


#define RGB         0u
#define LINRGB      1u

#define pair(u,v) ((u)|((v) << 16))
#define first(p) ((p) & 0xffffu)
#define second(p) ((p) >> 16)

bool
do_conversion (in vec4 color, in uint f, out vec4 result)
{
  switch (f)
    {
    case pair (RGB, LINRGB): result = srgb_to_linear_srgb (color); break;
    case pair (LINRGB, RGB): result = linear_srgb_to_srgb (color); break;

    default: return false;
    }

  return true;
}

vec4
color_convert (in vec4 color, in uint f)
{
  vec4 result;

  if (f == 0u)
    return color;

  do_conversion (color, f, result);

  return result;
}

#endif /* _COLOR_ */

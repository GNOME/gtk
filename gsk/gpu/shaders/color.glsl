#ifndef _COLOR_
#define _COLOR_

#define COLOR_SPACE_OUTPUT_PREMULTIPLIED (1u << 2)
#define COLOR_SPACE_ALT_PREMULTIPLIED    (1u << 3)
#define COLOR_SPACE_OUTPUT_SHIFT 8u
#define COLOR_SPACE_ALT_SHIFT 16u
#define COLOR_SPACE_COLOR_STATE_MASK 0xFFu

#define OUTPUT_COLOR_SPACE ((GSK_COLOR_STATES >> COLOR_SPACE_OUTPUT_SHIFT) & COLOR_SPACE_COLOR_STATE_MASK)
#define ALT_COLOR_SPACE ((GSK_COLOR_STATES >> COLOR_SPACE_ALT_SHIFT) & COLOR_SPACE_COLOR_STATE_MASK)
#define OUTPUT_PREMULTIPLIED ((GSK_COLOR_STATES & COLOR_SPACE_OUTPUT_PREMULTIPLIED) == COLOR_SPACE_OUTPUT_PREMULTIPLIED)
#define ALT_PREMULTIPLIED ((GSK_COLOR_STATES & COLOR_SPACE_ALT_PREMULTIPLIED) == COLOR_SPACE_ALT_PREMULTIPLIED)

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
srgb_eotf (float v)
{
  if (v >= 0.04045)
    return pow (((v + 0.055) / (1.0 + 0.055)), 2.4);
  else
    return v / 12.92;
}

float
srgb_oetf (float v)
{
  if (v > 0.0031308)
    return 1.055 * pow (v, 1.0 / 2.4) - 0.055;
  else
    return 12.92 * v;
}

vec4
alt_color_alpha (vec4  color,
                 float alpha)
{
  if (ALT_PREMULTIPLIED)
    return color * alpha;
  else
    return vec4 (color.rgb, color.a * alpha);
}

vec4
output_color_alpha (vec4  color,
                    float alpha)
{
  if (OUTPUT_PREMULTIPLIED)
    return color * alpha;
  else
    return vec4 (color.rgb, color.a * alpha);
}

vec4
alt_color_from_output (vec4 color)
{
  if (OUTPUT_COLOR_SPACE == ALT_COLOR_SPACE)
    {
      if (OUTPUT_PREMULTIPLIED && !ALT_PREMULTIPLIED)
        return color_unpremultiply (color);
      else if (!OUTPUT_PREMULTIPLIED && ALT_PREMULTIPLIED)
        return color_premultiply (color);
      else
        return color;
    }

  if (OUTPUT_PREMULTIPLIED)
    color = color_unpremultiply (color);

  if (OUTPUT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB &&
      ALT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB_LINEAR)
    {
      color = vec4 (srgb_eotf (color.r),
                    srgb_eotf (color.g),
                    srgb_eotf (color.b),
                    color.a);
    }
  else if (OUTPUT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB_LINEAR &&
           ALT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB)
    {
      color = vec4 (srgb_oetf (color.r),
                    srgb_oetf (color.g),
                    srgb_oetf (color.b),
                    color.a);
    }

  if (ALT_PREMULTIPLIED)
    color = color_premultiply (color);

  return color;
}

vec4
output_color_from_alt (vec4 color)
{
  if (OUTPUT_COLOR_SPACE == ALT_COLOR_SPACE)
    {
      if (ALT_PREMULTIPLIED && !OUTPUT_PREMULTIPLIED)
        return color_unpremultiply (color);
      else if (!ALT_PREMULTIPLIED && OUTPUT_PREMULTIPLIED)
        return color_premultiply (color);
      else
        return color;
    }

  if (ALT_PREMULTIPLIED)
    color = color_unpremultiply (color);

  if (ALT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB &&
      OUTPUT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB_LINEAR)
    {
      color = vec4 (srgb_eotf (color.r),
                    srgb_eotf (color.g),
                    srgb_eotf (color.b),
                    color.a);
    }
  else if (ALT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB_LINEAR &&
           OUTPUT_COLOR_SPACE == GDK_COLOR_STATE_ID_SRGB)
    {
      color = vec4 (srgb_oetf (color.r),
                    srgb_oetf (color.g),
                    srgb_oetf (color.b),
                    color.a);
    }

  if (OUTPUT_PREMULTIPLIED)
    color = color_premultiply (color);

  return color;
}

float
luminance (vec3 color)
{
  return dot (vec3 (0.2126, 0.7152, 0.0722), color);
}

#endif /* _COLOR_ */

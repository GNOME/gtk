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

vec3
apply_eotf (vec3 color,
            uint cs)
{
  switch (cs)
    {
    case GDK_COLOR_STATE_ID_SRGB:
      return vec3 (srgb_eotf (color.r),
                   srgb_eotf (color.g),
                   srgb_eotf (color.b));

     default:
       return color;
    }
}

vec3
apply_oetf (vec3 color,
            uint cs)
{
  switch (cs)
    {
    case GDK_COLOR_STATE_ID_SRGB:
      return vec3 (srgb_oetf (color.r),
                   srgb_oetf (color.g),
                   srgb_oetf (color.b));

     default:
       return color;
    }
}

vec3
convert_linear (vec3 color,
                uint from,
                uint to)
{
  return color;
}

uint
linear_color_space (uint cs)
{
  return GDK_COLOR_STATE_ID_SRGB_LINEAR;
}

vec4
convert_color (vec4 color,
               uint from,
               bool from_premul,
               uint to,
               bool to_premul)
{
  if (from_premul && (!to_premul || from != to))
    color = color_unpremultiply (color);

  if (from != to)
    {
      uint from_linear = linear_color_space (from);
      uint to_linear = linear_color_space (to);

      if (from_linear != from)
        color.rgb = apply_eotf (color.rgb, from);

      if (from_linear != to_linear)
        color.rgb = convert_linear (color.rgb, from_linear, to_linear);

      if (to_linear != to)
        color.rgb = apply_oetf (color.rgb, to);
    }

  if (to_premul && (!from_premul || from != to))
    color = color_premultiply (color);

  return color;
}

vec4
alt_color_from_output (vec4 color)
{
  return convert_color (color,
                        OUTPUT_COLOR_SPACE, OUTPUT_PREMULTIPLIED,
                        ALT_COLOR_SPACE, ALT_PREMULTIPLIED);
}

vec4
output_color_from_alt (vec4 color)
{
  return convert_color (color,
                        ALT_COLOR_SPACE, ALT_PREMULTIPLIED,
                        OUTPUT_COLOR_SPACE, OUTPUT_PREMULTIPLIED);
}

float
luminance (vec3 color)
{
  return dot (vec3 (0.2126, 0.7152, 0.0722), color);
}

#endif /* _COLOR_ */

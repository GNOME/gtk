#ifndef GSK_LEGACY
precision highp float;
#endif

#if defined(GSK_GLES) || defined(GSK_LEGACY)
#define _OUT_ varying
#define _IN_ varying
#define _GSK_ROUNDED_RECT_UNIFORM_ vec4[3]
#else
#define _OUT_ out
#define _IN_ in
#define _GSK_ROUNDED_RECT_UNIFORM_ GskRoundedRect
#endif


struct GskRoundedRect
{
  vec4 bounds; // Top left and bottom right
  // Look, arrays can't be in structs if you want to return the struct
  // from a function in gles or whatever. Just kill me.
  vec4 corner_points1; // xy = top left, zw = top right
  vec4 corner_points2; // xy = bottom right, zw = bottom left
};

// Transform from a C GskRoundedRect to what we need.
GskRoundedRect
gsk_create_rect(vec4[3] data)
{
  vec4 bounds = vec4(data[0].xy, data[0].xy + data[0].zw);

  vec4 corner_points1 = vec4(bounds.xy + data[1].xy,
                             bounds.zy + vec2(data[1].zw * vec2(-1, 1)));
  vec4 corner_points2 = vec4(bounds.zw + (data[2].xy * vec2(-1, -1)),
                             bounds.xw + vec2(data[2].zw * vec2(1, -1)));

  return GskRoundedRect(bounds, corner_points1, corner_points2);
}

vec4
gsk_get_bounds(vec4[3] data)
{
  return vec4(data[0].xy, data[0].xy + data[0].zw);
}

vec4 gsk_premultiply(vec4 c)
{
  return vec4(c.rgb * c.a, c.a);
}

vec4 gsk_unpremultiply(vec4 c)
{
  if (c.a != 0)
    return vec4(c.rgb / c.a, c.a);
  else
    return c;
}

vec4 gsk_srgb_to_linear(vec4 srgb)
{
  vec3 linear_rgb = pow(srgb.rgb, vec3(2.2));
  return vec4(linear_rgb, srgb.a);
}

vec4 gsk_linear_to_srgb(vec4 linear_rgba)
{
  vec3 srgb = pow(linear_rgba.rgb, vec3(1/2.2));
  return vec4(srgb, linear_rgba.a);
}

vec4 gsk_scaled_premultiply(vec4 c, float s) {
  // Fast version of gsk_premultiply(c) * s
  // 4 muls instead of 7
  float a = s * c.a;

  return vec4(c.rgb * a, a);
}

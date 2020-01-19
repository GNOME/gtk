

#ifndef GSK_LEGACY
precision highp float;
#endif

#if GSK_GLES
#define _OUT_ varying
#define _IN_ varying
#define _ROUNDED_RECT_UNIFORM_ vec4[3]
#elif GSK_LEGACY
#define _OUT_ varying
#define _IN_ varying
#define _ROUNDED_RECT_UNIFORM_ vec4[3]

// HERE we need to pick something else than a struct...

#else
#define _OUT_ out
#define _IN_ in
#define _ROUNDED_RECT_UNIFORM_ RoundedRect
#endif


struct RoundedRect
{
  vec4 bounds;
  // Look, arrays can't be in structs if you want to return the struct
  // from a function in gles or whatever. Just kill me.
  vec4 corner_points1; // xy = top left, zw = top right
  vec4 corner_points2; // xy = bottom right, zw = bottom left
};

// Transform from a GskRoundedRect to a RoundedRect as we need it.
RoundedRect
create_rect(vec4[3] data)
{
  vec4 bounds = vec4(data[0].xy, data[0].xy + data[0].zw);

  vec4 corner_points1 = vec4(bounds.xy + data[1].xy,
                             bounds.zy + vec2(data[1].zw * vec2(-1, 1)));
  vec4 corner_points2 = vec4(bounds.zw + (data[2].xy * vec2(-1, -1)),
                             bounds.xw + vec2(data[2].zw * vec2(1, -1)));

  return RoundedRect(bounds, corner_points1, corner_points2);
}

precision highp float;

#if defined(GSK_GLES) && __VERSION__ < 310
layout(std140)
#else
layout(std140, binding = 0)
#endif
uniform PushConstants
{
    mat4 mvp;
    vec4 clip_bounds;
    vec4 clip_widths;
    vec4 clip_heights;
    vec2 scale;
} push;

#if defined(GSK_GLES) && __VERSION__ < 310
layout(std140)
#else
layout(std140, binding = 1)
#endif
uniform Floats
{
  vec4 really_just_floats[1024];
} floats;

uniform sampler2D textures[16];

#define GSK_VERTEX_INDEX gl_VertexID


#ifdef GSK_VERTEX_SHADER
#define IN(_loc) in
#define PASS(_loc) out
#define PASS_FLAT(_loc) flat out
#endif


#ifdef GSK_FRAGMENT_SHADER
#define PASS(_loc) in
#define PASS_FLAT(_loc) flat in

float
gsk_get_float (int id)
{
  return floats.really_just_floats[id >> 2][id & 3];
}

float
gsk_get_float (uint id)
{
  return gsk_get_float (int (id));
}

#define gsk_get_int(id) (floatBitsToInt(gsk_get_float(id)))
#define gsk_get_uint(id) (floatBitsToUint(gsk_get_float(id)))

#ifdef GSK_GLES
vec4
gsk_texture (uint id,
             vec2 pos)
{
  switch(id)
  {
    case 0u:
      return texture (textures[0], pos);
    case 1u:
      return texture (textures[1], pos);
    case 2u:
      return texture (textures[2], pos);
    case 3u:
      return texture (textures[3], pos);
    case 4u:
      return texture (textures[4], pos);
    case 5u:
      return texture (textures[5], pos);
    case 6u:
      return texture (textures[6], pos);
    case 7u:
      return texture (textures[7], pos);
    case 8u:
      return texture (textures[8], pos);
    case 9u:
      return texture (textures[9], pos);
    case 10u:
      return texture (textures[10], pos);
    case 11u:
      return texture (textures[11], pos);
    case 12u:
      return texture (textures[12], pos);
    case 13u:
      return texture (textures[13], pos);
    case 14u:
      return texture (textures[14], pos);
    case 15u:
      return texture (textures[15], pos);
    default:
      return vec4 (1.0, 0.0, 0.8, 1.0);
  }
}

#else /* !GSK_GLES */

#define gsk_texture(id, pos) texture (textures[id], pos)

#endif

layout(location = 0) out vec4 out_color;
void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#endif

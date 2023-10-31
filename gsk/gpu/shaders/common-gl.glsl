precision highp float;

#if defined(GSK_GLES) && __VERSION__ < 310
layout(std140)
#else
layout(std140, binding = 0)
#endif
uniform PushConstants
{
    mat4 mvp;
    mat3x4 clip;
    vec2 scale;
} push;

#define GSK_GLOBAL_MVP push.mvp
#define GSK_GLOBAL_CLIP push.clip
#define GSK_GLOBAL_CLIP_RECT push.clip[0]
#define GSK_GLOBAL_SCALE push.scale

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

ivec2
gsk_texture_size (uint id,
                  int  lod)
{
  switch(id)
  {
    case 0u:
      return textureSize (textures[0], lod);
    case 1u:
      return textureSize (textures[1], lod);
    case 2u:
      return textureSize (textures[2], lod);
    case 3u:
      return textureSize (textures[3], lod);
    case 4u:
      return textureSize (textures[4], lod);
    case 5u:
      return textureSize (textures[5], lod);
    case 6u:
      return textureSize (textures[6], lod);
    case 7u:
      return textureSize (textures[7], lod);
    case 8u:
      return textureSize (textures[8], lod);
    case 9u:
      return textureSize (textures[9], lod);
    case 10u:
      return textureSize (textures[10], lod);
    case 11u:
      return textureSize (textures[11], lod);
    case 12u:
      return textureSize (textures[12], lod);
    case 13u:
      return textureSize (textures[13], lod);
    case 14u:
      return textureSize (textures[14], lod);
    case 15u:
      return textureSize (textures[15], lod);
    default:
      return ivec2 (1, 1);
  }
}

vec4
gsk_texel_fetch (uint  id,
                 ivec2 pos,
                 int   lod)
{
  switch(id)
  {
    case 0u:
      return texelFetch (textures[0], pos, lod);
    case 1u:
      return texelFetch (textures[1], pos, lod);
    case 2u:
      return texelFetch (textures[2], pos, lod);
    case 3u:
      return texelFetch (textures[3], pos, lod);
    case 4u:
      return texelFetch (textures[4], pos, lod);
    case 5u:
      return texelFetch (textures[5], pos, lod);
    case 6u:
      return texelFetch (textures[6], pos, lod);
    case 7u:
      return texelFetch (textures[7], pos, lod);
    case 8u:
      return texelFetch (textures[8], pos, lod);
    case 9u:
      return texelFetch (textures[9], pos, lod);
    case 10u:
      return texelFetch (textures[10], pos, lod);
    case 11u:
      return texelFetch (textures[11], pos, lod);
    case 12u:
      return texelFetch (textures[12], pos, lod);
    case 13u:
      return texelFetch (textures[13], pos, lod);
    case 14u:
      return texelFetch (textures[14], pos, lod);
    case 15u:
      return texelFetch (textures[15], pos, lod);
    default:
      return vec4 (1.0, 0.0, 0.8, 1.0);
  }
}

#else /* !GSK_GLES */

#define gsk_texture(id, pos) texture (textures[id], pos)
#define gsk_texture_size(id, lod) textureSize (textures[id], lod)
#define gsk_texel_fetch(id, pos, lod) texelFetch (textures[id], pos, lod)

#endif

layout(location = 0) out vec4 out_color;
void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#endif

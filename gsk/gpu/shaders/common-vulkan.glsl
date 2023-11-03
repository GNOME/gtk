#ifdef HAVE_NONUNIFORM
#extension GL_EXT_nonuniform_qualifier : enable
#endif

#include "enums.glsl"

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat3x4 clip;
    vec2 scale;
} push;

layout(constant_id=0) const uint GSK_SHADER_CLIP = GSK_GPU_SHADER_CLIP_NONE;
layout(constant_id=1) const uint GSK_N_IMMUTABLE_SAMPLERS = 8;
layout(constant_id=2) const uint GSK_N_SAMPLERS = 16;
layout(constant_id=3) const uint GSK_N_BUFFERS = 16;

#define GSK_GLOBAL_MVP push.mvp
#define GSK_GLOBAL_CLIP push.clip
#define GSK_GLOBAL_CLIP_RECT push.clip[0]
#define GSK_GLOBAL_SCALE push.scale

#define GSK_VERTEX_INDEX gl_VertexIndex

#ifdef GSK_VERTEX_SHADER
#define IN(_loc) layout(location = _loc) in
#define PASS(_loc) layout(location = _loc) out
#define PASS_FLAT(_loc) layout(location = _loc) flat out
#endif


#ifdef GSK_FRAGMENT_SHADER
#define PASS(_loc) layout(location = _loc) in
#define PASS_FLAT(_loc) layout(location = _loc) flat in

layout(set = 0, binding = 0) uniform sampler2D immutable_textures[GSK_N_IMMUTABLE_SAMPLERS];
layout(set = 0, binding = 1) uniform sampler2D textures[GSK_N_SAMPLERS];
layout(set = 1, binding = 0) readonly buffer FloatBuffers {
  float floats[];
} buffers[GSK_N_BUFFERS];

layout(location = 0) out vec4 out_color;

vec4
gsk_texture (uint id,
             vec2 pos)
{
  if ((id & 1) != 0)
    {
      id >>= 1;
      if (id == 0)
        return texture (immutable_textures[0], pos);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 1 && id == 1)
        return texture (immutable_textures[1], pos);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 2 && id == 2)
        return texture (immutable_textures[2], pos);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 3 && id == 3)
        return texture (immutable_textures[3], pos);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 4 && id == 4)
        return texture (immutable_textures[4], pos);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 5 && id == 5)
        return texture (immutable_textures[5], pos);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 6 && id == 6)
        return texture (immutable_textures[6], pos);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 7 && id == 7)
        return texture (immutable_textures[7], pos);
    }
  else
    {
      id >>= 1;
#ifdef HAVE_NONUNIFORM
      return texture (textures[nonuniformEXT (id)], pos);
#else
      if (id == 0)
        return texture (textures[0], pos);
      else if (GSK_N_SAMPLERS > 1 && id == 1)
        return texture (textures[1], pos);
      else if (GSK_N_SAMPLERS > 2 && id == 2)
        return texture (textures[2], pos);
      else if (GSK_N_SAMPLERS > 3 && id == 3)
        return texture (textures[3], pos);
      else if (GSK_N_SAMPLERS > 4 && id == 4)
        return texture (textures[4], pos);
      else if (GSK_N_SAMPLERS > 5 && id == 5)
        return texture (textures[5], pos);
      else if (GSK_N_SAMPLERS > 6 && id == 6)
        return texture (textures[6], pos);
      else if (GSK_N_SAMPLERS > 7 && id == 7)
        return texture (textures[7], pos);
      else if (GSK_N_SAMPLERS > 8)
        return texture (textures[id], pos);
#endif
    }
  return vec4 (1.0, 0.0, 0.8, 1.0);
}

ivec2
gsk_texture_size (uint id,
                  int  lod)
{
  if ((id & 1) != 0)
    {
      id >>= 1;
      if (id == 0)
        return textureSize (immutable_textures[0], lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 1 && id == 1)
        return textureSize (immutable_textures[1], lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 2 && id == 2)
        return textureSize (immutable_textures[2], lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 3 && id == 3)
        return textureSize (immutable_textures[3], lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 4 && id == 4)
        return textureSize (immutable_textures[4], lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 5 && id == 5)
        return textureSize (immutable_textures[5], lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 6 && id == 6)
        return textureSize (immutable_textures[6], lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 7 && id == 7)
        return textureSize (immutable_textures[7], lod);
    }
  else
    {
      id >>= 1;
#ifdef HAVE_NONUNIFORM
      return textureSize (textures[nonuniformEXT (id)], lod);
#else
      if (id == 0)
        return textureSize (textures[0], lod);
      else if (GSK_N_SAMPLERS > 1 && id == 1)
        return textureSize (textures[1], lod);
      else if (GSK_N_SAMPLERS > 2 && id == 2)
        return textureSize (textures[2], lod);
      else if (GSK_N_SAMPLERS > 3 && id == 3)
        return textureSize (textures[3], lod);
      else if (GSK_N_SAMPLERS > 4 && id == 4)
        return textureSize (textures[4], lod);
      else if (GSK_N_SAMPLERS > 5 && id == 5)
        return textureSize (textures[5], lod);
      else if (GSK_N_SAMPLERS > 6 && id == 6)
        return textureSize (textures[6], lod);
      else if (GSK_N_SAMPLERS > 7 && id == 7)
        return textureSize (textures[7], lod);
      else if (GSK_N_SAMPLERS > 8)
        return textureSize (textures[id], lod);
#endif
    }
  return ivec2 (1, 1);
}

vec4
gsk_texel_fetch (uint  id,
                 ivec2 pos,
                 int   lod)
{
  if ((id & 1) != 0)
    {
      id >>= 1;
      if (id == 0)
        return texelFetch (immutable_textures[0], pos, lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 1 && id == 1)
        return texelFetch (immutable_textures[1], pos, lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 2 && id == 2)
        return texelFetch (immutable_textures[2], pos, lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 3 && id == 3)
        return texelFetch (immutable_textures[3], pos, lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 4 && id == 4)
        return texelFetch (immutable_textures[4], pos, lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 5 && id == 5)
        return texelFetch (immutable_textures[5], pos, lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 6 && id == 6)
        return texelFetch (immutable_textures[6], pos, lod);
      else if (GSK_N_IMMUTABLE_SAMPLERS > 7 && id == 7)
        return texelFetch (immutable_textures[7], pos, lod);
    }
  else
    {
      id >>= 1;
#ifdef HAVE_NONUNIFORM
      return texelFetch (textures[nonuniformEXT (id)], pos, lod);
#else
      if (id == 0)
        return texelFetch (textures[0], pos, lod);
      else if (GSK_N_SAMPLERS > 1 && id == 1)
        return texelFetch (textures[1], pos, lod);
      else if (GSK_N_SAMPLERS > 2 && id == 2)
        return texelFetch (textures[2], pos, lod);
      else if (GSK_N_SAMPLERS > 3 && id == 3)
        return texelFetch (textures[3], pos, lod);
      else if (GSK_N_SAMPLERS > 4 && id == 4)
        return texelFetch (textures[4], pos, lod);
      else if (GSK_N_SAMPLERS > 5 && id == 5)
        return texelFetch (textures[5], pos, lod);
      else if (GSK_N_SAMPLERS > 6 && id == 6)
        return texelFetch (textures[6], pos, lod);
      else if (GSK_N_SAMPLERS > 7 && id == 7)
        return texelFetch (textures[7], pos, lod);
      else if (GSK_N_SAMPLERS > 8)
        return texelFetch (textures[id], pos, lod);
#endif
    }
  return vec4 (1.0, 0.0, 0.8, 1.0);
}

#ifdef HAVE_NONUNIFORM
#define gsk_get_buffer(id) buffers[nonuniformEXT (id)]
#else
#define gsk_get_buffer(id) buffers[id]
#endif
#define gsk_get_float(id) gsk_get_buffer(id >> 22).floats[id & 0x3FFFFF]
#define gsk_get_int(id) (floatBitsToInt(gsk_get_float(id)))
#define gsk_get_uint(id) (floatBitsToUint(gsk_get_float(id)))

void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#endif

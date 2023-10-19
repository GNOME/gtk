#extension GL_EXT_nonuniform_qualifier : enable

#include "enums.glsl"

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat3x4 clip;
    vec2 scale;
} push;

layout(constant_id=0) const uint GSK_SHADER_CLIP = GSK_GPU_SHADER_CLIP_NONE;

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

layout(set = 0, binding = 1) uniform sampler2D textures[50000];
layout(set = 1, binding = 0) readonly buffer FloatBuffers {
  float floats[];
} buffers[50000];

layout(location = 0) out vec4 out_color;

#define gsk_texture(id, pos) texture (textures[nonuniformEXT (id)], pos)
#define gsk_get_buffer(id) buffers[nonuniformEXT (id)]
#define gsk_get_float(id) gsk_get_buffer(0).floats[id]
#define gsk_get_int(id) (floatBitsToInt(gsk_get_float(id)))
#define gsk_get_uint(id) (floatBitsToUint(gsk_get_float(id)))

void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#endif

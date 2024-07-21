#include "enums.glsl"

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat3x4 clip;
    vec2 scale;
} push;

layout(constant_id=0) const uint GSK_FLAGS = 0;
layout(constant_id=1) const uint GSK_COLOR_STATES = 0;
layout(constant_id=2) const uint GSK_VARIATION = 0;

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

layout(set = 0, binding = 0) uniform sampler2D GSK_TEXTURE0;
layout(set = 1, binding = 0) uniform sampler2D GSK_TEXTURE1;

layout(location = 0) out vec4 out_color;

void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#endif

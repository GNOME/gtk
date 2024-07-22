precision highp float;

#if __VERSION__ < 420 || (defined(GSK_GLES) && __VERSION__ < 310)
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

#define GSK_VERTEX_INDEX gl_VertexID


#ifdef GSK_VERTEX_SHADER
#define IN(_loc) in
#define PASS(_loc) out
#define PASS_FLAT(_loc) flat out
#endif


#ifdef GSK_FRAGMENT_SHADER
#define PASS(_loc) in
#define PASS_FLAT(_loc) flat in

#if GSK_N_TEXTURES > 0

#ifdef GSK_TEXTURE0_IS_EXTERNAL
uniform samplerExternalOES GSK_TEXTURE0;
#else
uniform sampler2D GSK_TEXTURE0;
#endif

#if GSK_N_TEXTURES > 1

#ifdef GSK_TEXTURE1_IS_EXTERNAL
uniform samplerExternalOES GSK_TEXTURE1;
#else
uniform sampler2D GSK_TEXTURE1;
#endif

#endif
#endif

layout(location = 0) out vec4 out_color;
void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#endif

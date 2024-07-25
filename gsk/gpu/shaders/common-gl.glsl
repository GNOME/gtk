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
#define HAS_EXTERNAL_TEXTURES
#else
uniform sampler2D GSK_TEXTURE0;
#endif

#if GSK_N_TEXTURES > 1

#ifdef GSK_TEXTURE1_IS_EXTERNAL
uniform samplerExternalOES GSK_TEXTURE1;
#ifndef HAS_EXTERNAL_TEXTURES
#define HAS_EXTERNAL_TEXTURES
#endif
#else
uniform sampler2D GSK_TEXTURE1;
#endif

#endif
#endif

#ifdef HAS_EXTERNAL_TEXTURES
vec4
gsk_texture_straight_alpha (samplerExternalOES tex,
                            vec2               pos)
{
  vec2 size = vec2 (textureSize (tex, 0));
  pos *= size;
  size -= vec2 (1.0);
  /* GL_CLAMP_TO_EDGE */
  pos = clamp (pos - 0.5, vec2 (0.0), size);
  ivec2 ipos = ivec2 (pos);
  pos = fract (pos);
  vec4 tl = texelFetch (tex, ipos, 0);
  tl.rgb *= tl.a;
  vec4 tr = texelFetch (tex, ipos + ivec2(1, 0), 0);
  tr.rgb *= tr.a;
  vec4 bl = texelFetch (tex, ipos + ivec2(0, 1), 0);
  bl.rgb *= bl.a;
  vec4 br = texelFetch (tex, ipos + ivec2(1, 1), 0);
  br.rgb *= br.a;
  return mix (mix (tl, tr, pos.x), mix (bl, br, pos.x), pos.y);
}
#endif

layout(location = 0) out vec4 out_color;
void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#endif

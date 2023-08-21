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

#define GSK_VERTEX_INDEX gl_VertexID


#ifdef GSK_VERTEX_SHADER
#define IN(_loc) in
#define PASS(_loc) out
#define PASS_FLAT(_loc) flat out
#endif


#ifdef GSK_FRAGMENT_SHADER
#define PASS(_loc) in
#define PASS_FLAT(_loc) flat in

uniform sampler2D textures[16];
#define gsk_get_texture(id) textures[id]

#ifdef GSK_GLES
void
gsk_set_output_color (vec4 color)
{
  gl_FragColor = color;
}
#else
layout(location = 0) out vec4 out_color;
void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}
#endif

#endif

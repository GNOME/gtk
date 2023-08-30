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

#define gsk_get_texture(id) textures[id]
#define gsk_get_int(id) (floatBitsToInt(gsk_get_float(id)))
#define gsk_get_uint(id) (floatBitsToUint(gsk_get_float(id)))

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

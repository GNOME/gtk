#if 1
#extension GL_EXT_nonuniform_qualifier : enable
#endif 

#include "enums.glsl"

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat3x4 clip;
    vec2 scale;
} push;

layout(constant_id=0) const uint GSK_FLAGS = 0;
layout(constant_id=1) const uint GSK_COLOR_STATES = 0;
layout(constant_id=2) const uint GSK_VARIATION = 0;
layout(constant_id=3) const uint GSK_N_BUFFERS = 32;

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

layout(set = 0, binding = 0) uniform sampler2D GSK_TEXTURE0_ARRAY[3];
#define GSK_TEXTURE0 GSK_TEXTURE0_ARRAY[0]
#define GSK_TEXTURE0_1 GSK_TEXTURE0_ARRAY[1]
#define GSK_TEXTURE0_2 GSK_TEXTURE0_ARRAY[2]
layout(set = 1, binding = 0) uniform sampler2D GSK_TEXTURE1_ARRAY[3];
#define GSK_TEXTURE1 GSK_TEXTURE1_ARRAY[0]
#define GSK_TEXTURE1_1 GSK_TEXTURE1_ARRAY[1]
#define GSK_TEXTURE1_2 GSK_TEXTURE1_ARRAY[2]

layout(set = 1, binding = 0) readonly buffer FloatBuffers {
  float floats[];
} buffers[GSK_N_BUFFERS];

layout(location = 0) out vec4 out_color;

void
gsk_set_output_color (vec4 color)
{
  out_color = color;
}

#if 1
#define gsk_get_buffer(id) buffers[nonuniformEXT (id)]
#define gsk_get_float(id) gsk_get_buffer(id >> 22).floats[id & 0x3FFFFF]
#else
float
gsk_get_float (uint id)
{
  uint buffer_id = id >> 22;
  uint float_id = id & 0x3FFFFF;
  if (buffer_id < 16)
    {
      if (buffer_id < 8)
        {
          if (buffer_id < 4)
            {
              if (buffer_id < 2)
                {
                  if (buffer_id < 1)
                    return buffers[0].floats[float_id];
                  else if (GSK_N_BUFFERS > 1)
                    return buffers[1].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 2)
                {
                  if (buffer_id < 3)
                    return buffers[2].floats[float_id];
                  else if (GSK_N_BUFFERS > 3)
                    return buffers[3].floats[float_id];
                }
            }
          else if (GSK_N_BUFFERS > 4)
            {
              if (buffer_id < 6)
                {
                  if (buffer_id < 5)
                    return buffers[4].floats[float_id];
                  else if (GSK_N_BUFFERS > 5)
                    return buffers[5].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 6)
                {
                  if (buffer_id < 7)
                    return buffers[6].floats[float_id];
                  else if (GSK_N_BUFFERS > 7)
                    return buffers[7].floats[float_id];
                }
            }
        }
      else if (GSK_N_BUFFERS > 8)
        {
          if (buffer_id < 12)
            {
              if (buffer_id < 10)
                {
                  if (buffer_id < 9)
                    return buffers[8].floats[float_id];
                  else if (GSK_N_BUFFERS > 9)
                    return buffers[9].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 10)
                {
                  if (buffer_id < 11)
                    return buffers[10].floats[float_id];
                  else if (GSK_N_BUFFERS > 11)
                    return buffers[11].floats[float_id];
                }
            }
          else if (GSK_N_BUFFERS > 12)
            {
              if (buffer_id < 14)
                {
                  if (buffer_id < 13)
                    return buffers[12].floats[float_id];
                  else if (GSK_N_BUFFERS > 13)
                    return buffers[13].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 14)
                {
                  if (buffer_id < 15)
                    return buffers[14].floats[float_id];
                  else if (GSK_N_BUFFERS > 15)
                    return buffers[15].floats[float_id];
                }
            }
        }
    }
  else if (GSK_N_BUFFERS > 16)
    {
      if (buffer_id < 24)
        {
          if (buffer_id < 20)
            {
              if (buffer_id < 18)
                {
                  if (buffer_id < 17)
                    return buffers[16].floats[float_id];
                  else if (GSK_N_BUFFERS > 17)
                    return buffers[17].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 18)
                {
                  if (buffer_id < 19)
                    return buffers[18].floats[float_id];
                  else if (GSK_N_BUFFERS > 19)
                    return buffers[19].floats[float_id];
                }
            }
          else if (GSK_N_BUFFERS > 20)
            {
              if (buffer_id < 22)
                {
                  if (buffer_id < 21)
                    return buffers[20].floats[float_id];
                  else if (GSK_N_BUFFERS > 21)
                    return buffers[21].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 22)
                {
                  if (buffer_id < 23)
                    return buffers[22].floats[float_id];
                  else if (GSK_N_BUFFERS > 23)
                    return buffers[23].floats[float_id];
                }
            }
        }
      else if (GSK_N_BUFFERS > 24)
        {
          if (buffer_id < 28)
            {
              if (buffer_id < 26)
                {
                  if (buffer_id < 25)
                    return buffers[24].floats[float_id];
                  else if (GSK_N_BUFFERS > 25)
                    return buffers[25].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 26)
                {
                  if (buffer_id < 27)
                    return buffers[26].floats[float_id];
                  else if (GSK_N_BUFFERS > 27)
                    return buffers[27].floats[float_id];
                }
            }
          else if (GSK_N_BUFFERS > 28)
            {
              if (buffer_id < 30)
                {
                  if (buffer_id < 29)
                    return buffers[28].floats[float_id];
                  else if (GSK_N_BUFFERS > 29)
                    return buffers[29].floats[float_id];
                }
              else if (GSK_N_BUFFERS > 30)
                {
                  if (buffer_id < 31)
                    return buffers[30].floats[float_id];
                  else if (GSK_N_BUFFERS > 31)
                    return buffers[31].floats[float_id];
                }
            }
        }
    }
  return 0.0;
}
#endif
#define gsk_get_int(id) (floatBitsToInt(gsk_get_float(id)))
#define gsk_get_uint(id) (floatBitsToUint(gsk_get_float(id)))

#endif

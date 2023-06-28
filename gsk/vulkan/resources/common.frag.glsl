layout(set = 0, binding = 0) uniform sampler2D textures[50000];
layout(set = 1, binding = 0) readonly buffer FloatBuffers {
  float floats[];
} buffers[50000];

#define get_sampler(id) textures[id]
#define get_buffer(id) buffers[id]
#define get_float(id) get_buffer(0).floats[id]


layout(set = 0, binding = 0) uniform texture2D textures[50000];
layout(set = 1, binding = 0) uniform sampler samplers[50000];
layout(set = 2, binding = 0) readonly buffer FloatBuffers {
  float floats[];
} buffers[50000];

#define get_sampler(id) sampler2D(textures[id.x], samplers[id.y])
#define get_buffer(id) buffers[id]
#define get_float(id) get_buffer(0).floats[id]


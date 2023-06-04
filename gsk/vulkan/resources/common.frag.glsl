
layout(set = 0, binding = 0) uniform texture2D textures[50000];
layout(set = 0, binding = 1) uniform sampler samplers[50000];

#define get_sampler(id) sampler2D(textures[id.x], samplers[id.y])


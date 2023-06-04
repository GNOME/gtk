#ifndef _CONSTANTS_
#define _CONSTANTS_

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 clip_bounds;
    vec4 clip_widths;
    vec4 clip_heights;
    vec2 scale;
} push;

#endif

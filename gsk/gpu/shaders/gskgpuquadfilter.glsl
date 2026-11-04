#ifdef GSK_PREAMBLE
textures = 1;
var_name = "gsk_gpu_quad_filter";
struct_name = "GskGpuQuadFilter";
acs_equals_ccs = true;
acs_premultiplied = true;
opacity = true;

graphene_rect_t bounds;
graphene_rect_t tex_rect;
#endif /* GSK_PREAMBLE */

#include "gskgpuquadfilterinstance.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _bounds;
PASS(2) vec2 _tex_coord;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect b = rect_from_gsk (in_bounds);
  
  pos = rect_get_position (b);

  _pos = pos;
  _bounds = b;
  _tex_coord = rect_get_coord (rect_from_gsk (in_tex_rect), pos);
}

#endif



#ifdef GSK_FRAGMENT_SHADER

void
run (out vec4 color,
     out vec2 position)
{
  vec2 size = vec2 (textureSize (GSK_TEXTURE0, 0));
  vec2 texel = floor (_tex_coord * size - 0.5);
  vec2 f = fract (_tex_coord * size - 0.5);
  vec2 f2 = f1 * f1;
  vec2 f3 = f2 * f1;

#if 1
  float a = -0.5;
  mat4 matrix = mat4 ( a,       -2.0 * a,        a,   0,
                       a + 2.0,      - a - 3.0,  0.0, 1,
                      -a - 2.0,  2.0 * a + 3.0, -a,   0,
                      -a,              a,        0.0, 0);
#else
  mat4 matrix = mat4 (0.0, 0.0, 0.0, 0.0,
                      0.0, 0.0,-1.0, 1.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 0.0);
#endif

  vec4 wx = vec4 (f3.x, f2.x, f1.x, 1.0) * matrix;
  vec4 wy = vec4 (f3.y, f2.y, f1.y, 1.0) * matrix;
  vec2 gx = vec2 (wx[0] + wx[1], wx[2] + wx[3]);
  vec2 gy = vec2 (wy[0] + wy[1], wy[2] + wy[3]);
  
  color = vec4(0.0);
  color += gx[0] * gy[0] * texture (GSK_TEXTURE0, (texel + vec2 (0.5 - wx[0] / gx[0], 0.5 - wy[0] / gy[0])) / size);
  color += gx[0] * gy[1] * texture (GSK_TEXTURE0, (texel + vec2 (0.5 - wx[0] / gx[0], 1.5 + wy[3] / gy[1])) / size);
  color += gx[1] * gy[0] * texture (GSK_TEXTURE0, (texel + vec2 (1.5 + wx[3] / gx[1], 0.5 - wy[0] / gy[0])) / size);
  color += gx[1] * gy[1] * texture (GSK_TEXTURE0, (texel + vec2 (1.5 + wx[3] / gx[1], 1.5 + wy[3] / gy[1])) / size);
  position = _pos;
}

#endif

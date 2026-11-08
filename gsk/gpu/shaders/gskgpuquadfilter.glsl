#ifdef GSK_PREAMBLE
textures = 1;
var_name = "gsk_gpu_quad_filter";
struct_name = "GskGpuQuadFilter";
acs_equals_ccs = true;
acs_premultiplied = true;
opacity = true;

graphene_rect_t bounds;
graphene_rect_t tex_rect;

variation: GskQuadKernel hkernel;
variation: GskQuadKernel vkernel;
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

vec4
get_weights_cubic (float b,
                   float c,
                   float f)
{
  mat4 matrix = mat4 ( - b / 6.0 - c,          0.5 * b + 2.0 * c,       - 0.5 * b - c,   1.0 / 6.0 * b, 
                       - 1.5 * b - c + 2.0,    2.0 * b +       c - 3.0,           0.0, - 1.0 / 3.0 * b + 1.0,
                         1.5 * b + c - 2.0,  - 2.5 * b - 2.0 * c + 3.0,   0.5 * b + c,   1.0 / 6.0 * b,
                         b / 6.0 + c,                        - c,                 0.0,                   0.0);
  float f2 = f * f;
  float f3 = f2 * f;
  return vec4 (f3, f2, f, 1.0) * matrix;
}

vec4
get_weights_gaussian (float f)
{
  return vec4 ((1.0 - f) * (1.0 - f) * (1.0 - f) / 6.0,
               (3.0 * f * f * f - 6.0 * f * f + 4.0) / 6.0,
               (-3.0 * f * f * f + 3.0 * f * f + 3.0 * f + 1.0) / 6.0,
               f * f * f / 6.0);
}

vec4
get_weights (float f,
             uint variation)
{
  switch (variation)
  {
    case GSK_QUAD_KERNEL_CATMULL_ROM:
      return get_weights_cubic (0.0, 0.5, f);
    case GSK_QUAD_KERNEL_MITCHELL_NETRAVALI:
      return get_weights_cubic (1.0 / 3.0, 1.0 / 3.0, f);
    case GSK_QUAD_KERNEL_GAUSSIAN:
      return get_weights_gaussian (f);
    case GSK_QUAD_KERNEL_LINEAR:
    default:
      return vec4(0.0, f, 1.0 - f, 0.0);
  }
}

void
run (out vec4 color,
     out vec2 position)
{
  vec2 size = vec2 (textureSize (GSK_TEXTURE0, 0));
  vec2 texel = floor (_tex_coord * size - 0.5);
  vec2 f = fract (_tex_coord * size - 0.5);

  vec4 wx = get_weights (f.x, VARIATION_HKERNEL);
  vec4 wy = get_weights (f.y, VARIATION_VKERNEL);
  vec2 gx = vec2 (wx[0] + wx[1], wx[2] + wx[3]);
  vec2 gy = vec2 (wy[0] + wy[1], wy[2] + wy[3]);
  
  color = vec4(0.0);
  color += gx[0] * gy[0] * texture (GSK_TEXTURE0, (texel + vec2 (0.5 - wx[0] / gx[0], 0.5 - wy[0] / gy[0])) / size, -0.5);
  color += gx[0] * gy[1] * texture (GSK_TEXTURE0, (texel + vec2 (0.5 - wx[0] / gx[0], 1.5 + wy[3] / gy[1])) / size, -0.5);
  color += gx[1] * gy[0] * texture (GSK_TEXTURE0, (texel + vec2 (1.5 + wx[3] / gx[1], 0.5 - wy[0] / gy[0])) / size, -0.5);
  color += gx[1] * gy[1] * texture (GSK_TEXTURE0, (texel + vec2 (1.5 + wx[3] / gx[1], 1.5 + wy[3] / gy[1])) / size, -0.5);
  position = _pos;
}

#endif

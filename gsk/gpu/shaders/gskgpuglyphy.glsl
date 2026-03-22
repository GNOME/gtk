#ifdef GSK_PREAMBLE
textures = 1;

graphene_rect_t bounds;
graphene_vec4_t extents;
graphene_vec4_t atlas_info;
GdkColor color;
#endif /* GSK_PREAMBLE */

#include "gskgpuglyphyinstance.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _bounds;
PASS_FLAT(2) vec4 _extents;
PASS_FLAT(3) vec4 _atlas_info;
PASS_FLAT(4) vec4 _color;

#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect b = rect_from_gsk (in_bounds);

  pos = rect_get_position (b);

  _pos = pos;
  _bounds = b;
  _extents = in_extents;
  _atlas_info = in_atlas_info;
  _color = output_color_from_alt (in_color);
}

#endif

#ifdef GSK_FRAGMENT_SHADER

ivec4
glyphy_atlas_fetch (int index)
{
  int item_w = int (_atlas_info.z + 0.5);
  int x = index % item_w;
  int y = index / item_w;
  x += int (_atlas_info.x + 0.5);
  y += int (_atlas_info.y + 0.5);
  vec4 raw = texelFetch (GSK_TEXTURE0, ivec2 (x, y), 0);
  ivec4 u16 = ivec4 (raw * 65535.0 + 0.5);
  return u16 - ivec4 (32768);
}

#include "glyphy-fragment.glsl"

void
run (out vec4 color,
     out vec2 position)
{
  vec2 size = rect_size (_bounds);

  if (size.x <= 0.0 || size.y <= 0.0)
    {
      color = vec4 (0.0);
      position = _pos;
      return;
    }

  vec2 t = (_pos - rect_pos (_bounds)) / size;
  vec2 render_coord = vec2 (mix (_extents.x, _extents.z, t.x),
                            mix (_extents.w, _extents.y, t.y));
  float coverage = glyphy_render (render_coord, 0u);

  color = output_color_alpha (_color, coverage);
  position = _pos;
}

#endif

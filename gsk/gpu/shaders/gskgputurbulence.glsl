#ifdef GSK_PREAMBLE
textures = 1;
acs_equals_ccs = false;
acs_premultiplied = true;

graphene_rect_t bounds;
graphene_size_t base_frequency;
guint32 num_octaves;
float tile_x;
float tile_y;
float tile_width;
float tile_height;

variation: gboolean fractal_noise;
variation: gboolean stitch_tiles;
#endif /* GSK_PREAMBLE */

#include "gskgputurbulenceinstance.glsl"

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _bounds;
PASS_FLAT(2) vec2 _base_frequency;
PASS_FLAT(3) uint _num_octaves;
PASS_FLAT(4) float _tile_x;
PASS_FLAT(5) float _tile_y;
PASS_FLAT(6) float _tile_width;
PASS_FLAT(7) float _tile_height;


#ifdef GSK_VERTEX_SHADER

void
run (out vec2 pos)
{
  Rect b = rect_from_gsk (in_bounds);

  pos = rect_get_position (b);

  _pos = pos;
  _bounds = b;
  _base_frequency = in_base_frequency;
  _num_octaves = in_num_octaves;
  _tile_x = in_tile_x;
  _tile_y = in_tile_y;
  _tile_width = in_tile_width;
  _tile_height = in_tile_height;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

#define BSize 0x100
#define BM 0xff
#define PerlinN 0x1000

int
get_lattice (int idx)
{
  return int (texelFetch (GSK_TEXTURE0, ivec2 (idx, 0), 0).a);
}

vec2
get_gradient (int channel, int idx)
{
  return texelFetch (GSK_TEXTURE0, ivec2 (idx, channel), 0).rg;
}

float
s_curve (float t)
{
  return t * t * (3.0 - 2.0 * t);
}

vec4
noise2 (vec2 pos, int stitch_width, int stitch_height, int stitch_wrap_x, int stitch_wrap_y)
{
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  float rx0, rx1, ry0, ry1, sx, sy, a, b, u, v;
  int i, j;
  vec2 q;
  vec4 res;

  float fx = pos.x + float(PerlinN);
  bx0 = int (fx);
  bx1 = bx0 + 1;
  rx0 = fx - floor (fx);
  rx1 = rx0 - 1.0;

  float fy = pos.y + float(PerlinN);
  by0 = int (fy);
  by1 = by0 + 1;
  ry0 = fy - floor (fy);
  ry1 = ry0 - 1.0;

  if (VARIATION_STITCH_TILES)
    {
      if (bx0 >= stitch_wrap_x)
        bx0 -= stitch_width;
      if (bx1 >= stitch_wrap_x)
        bx1 -= stitch_width;
      if (by0 >= stitch_wrap_y)
        by0 -= stitch_height;
      if (by1 >= stitch_wrap_y)
        by1 -= stitch_height;
    }

  bx0 &= BM;
  bx1 &= BM;
  by0 &= BM;
  by1 &= BM;

  i = get_lattice (bx0);
  j = get_lattice (bx1);
  b00 = get_lattice (i + by0);
  b10 = get_lattice (j + by0);
  b01 = get_lattice (i + by1);
  b11 = get_lattice (j + by1);

  sx = s_curve (rx0);
  sy = s_curve (ry0);

  for (int channel = 0; channel < 4; channel++)
    {
      q = get_gradient (channel, b00);
      u = rx0 * q.x + ry0 * q.y;
      q = get_gradient (channel, b10);
      v = rx1 * q.x + ry0 * q.y;
      a = mix (u, v, sx);
      q = get_gradient (channel, b01);
      u = rx0 * q.x + ry1 * q.y;
      q = get_gradient (channel, b11);
      v = rx1 * q.x + ry1 * q.y;
      b = mix (u, v, sx);

      res[channel] = mix (a, b, sy);
    }

  return res;
}

vec4
turbulence (vec2 point, vec2 base_freq, uint n_octaves)
{
  int stitch_width = 0, stitch_height = 0, strich_wrap_x = 0, stitch_wrap_y = 0;

  if (VARIATION_STITCH_TILES)
    {
      if (base_freq.x != 0.0)
        {
          float freq_low = floor (_tile_width * base_freq.x) / _tile_width;
          float freq_high = ceil (_tile_width * base_freq.x) / _tile_width;
          if (base_freq.x / freq_low < freq_high / base_freq.x)
            base_freq.x = freq_low;
          else
            base_freq.x = freq_high;
        }
      if (base_freq.y != 0.0)
        {
          float freq_low = floor (_tile_height * base_freq.y) / _tile_height;
          float freq_high = ceil (_tile_height * base_freq.y) / _tile_height;
          if (base_freq.y / freq_low < freq_high / base_freq.y)
            base_freq.y = freq_low;
          else
            base_freq.y = freq_high;
        }
      stitch_width = int (_tile_width * base_freq.x + 0.5);
      strich_wrap_x = PerlinN + stitch_width;
      stitch_height = int (_tile_height * base_freq.y + 0.5);
      stitch_wrap_y = PerlinN + stitch_height;
    }

  vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);
  vec2 v = point * base_freq;
  float ratio = 1.0;

  for (uint octave = 0u; octave < n_octaves; octave++)
    {
      vec4 n = noise2 (v, stitch_width, stitch_height, strich_wrap_x, stitch_wrap_y);
      if (VARIATION_FRACTAL_NOISE)
        sum += n / ratio;
      else
        sum += abs (n) / ratio;
      v *= 2.0;
      ratio *= 2.0;
      if (VARIATION_STITCH_TILES)
        {
          stitch_width *= 2;
          strich_wrap_x = 2 * strich_wrap_x - PerlinN;
          stitch_height *= 2;
          stitch_wrap_y = 2 * stitch_wrap_y - PerlinN;
        }
    }

  return sum;
}

void
run (out vec4 color,
     out vec2 position)
{
  float alpha = rect_coverage (_bounds, _pos);
  /* Compute noise in node-local coordinates, not screen coordinates.
   * _pos includes the render pass offset which changes with scrolling
   * and window resizing. Subtract the quad origin to get the pixel
   * offset within the node, then add the node's own origin.
   */
  vec2 pixel_offset = (_pos - rect_bounds (_bounds).xy - 0.5 * fwidth(_pos)) / GSK_GLOBAL_SCALE;
  vec2 point = vec2 (_tile_x, _tile_y) + pixel_offset;

  vec4 n = turbulence (point, _base_frequency, _num_octaves);

  float r, g, b, a;

  if (VARIATION_FRACTAL_NOISE)
    {
      r = (n.r + 1.0) / 2.0;
      g = (n.g + 1.0) / 2.0;
      b = (n.b + 1.0) / 2.0;
      a = (n.a + 1.0) / 2.0;
    }
  else
    {
      r = n.r;
      g = n.g;
      b = n.b;
      a = n.a;
    }

  r = clamp (r, 0.0, 1.0);
  g = clamp (g, 0.0, 1.0);
  b = clamp (b, 0.0, 1.0);
  a = clamp (a, 0.0, 1.0);

  r *= a;
  g *= a;
  b *= a;

  color = vec4 (r, g, b, a);

  color = output_color_from_alt (color);
  color = output_color_alpha (color, alpha);
  position = _pos;
}

#endif

/* GSK - The GTK Scene Kit
 *
 * Copyright 2026  Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gskturbulencenodeprivate.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrenderreplay.h"

#include <tgmath.h>

/*< private >
 * GskTurbulenceNode:
 *
 * A render node generating a Perlin noise or fractal Brownian motion pattern.
 */
struct _GskTurbulenceNode
{
  GskRenderNode render_node;

  GdkColorState *color_state;
  graphene_size_t base_frequency;
  unsigned int num_octaves;
  int seed;
  GskNoiseType noise_type;
  gboolean stitch_tiles;
  GskRectSnap snap;

  float lookup[GSK_TURBULENCE_TABLE_WIDTH * GSK_TURBULENCE_TABLE_HEIGHT * 4];
};

/* We pack the Perlin noise lookup tables for a given seed into a
 * float array suitable for uploading as a 514×4 RGBA32F texture.
 *
 * Layout per texel at (i, row):
 *   Row 0: R=grad[0][i].x, G=grad[0][i].y, A=lattice[i]
 *   Row 1: R=grad[1][i].x, G=grad[1][i].y
 *   Row 2: R=grad[2][i].x, G=grad[2][i].y
 *   Row 3: R=grad[3][i].x, G=grad[3][i].y
 */

static inline int
get_lattice (const float *data,
             int          idx)
{
  return (int) data[idx * 4 + 3];
}

static inline void
set_lattice (float *data,
             int    idx,
             int    value)
{
  data[idx * 4 + 3] = (float) value;
}

static inline const float *
get_gradient (const float *data,
              int          channel,
              int          idx)
{
  return &data[channel * GSK_TURBULENCE_TABLE_WIDTH * 4 + idx * 4];
}

static inline void
set_gradient (float *data,
              int    channel,
              int    idx,
              float  x,
              float  y)
{
  data[channel * GSK_TURBULENCE_TABLE_WIDTH * 4 + idx * 4 + 0] = x;
  data[channel * GSK_TURBULENCE_TABLE_WIDTH * 4 + idx * 4 + 1] = y;
}

/* Perlin noise implementation based on W3C Filter Effects spec */

#define RAND_M 2147483647 /* 2**31 - 1 */
#define RAND_A 16807 /* 7**5; primitive root of m */
#define RAND_Q 127773 /* m / a */
#define RAND_R 2836 /* m % a */

#define BSize 0x100
#define BM 0xff
#define PerlinN 0x1000
#define NP 12 /* 2^PerlinN */
#define NM 0xfff

typedef struct {
  unsigned int width;
  unsigned int height;
  unsigned int wrap_x;
  unsigned int wrap_y;
} StitchInfo;

static int32_t
setup_seed (int32_t seed)
{
  if (seed <= 0)
    seed = -(seed % (RAND_M - 1)) + 1;
  if (seed > RAND_M - 1)
    seed = RAND_M - 1;
  return seed;
}

static int32_t
gsk_random (int32_t seed)
{
  long result;
  result = RAND_A * (seed % RAND_Q) - RAND_R * (seed / RAND_Q);
  if (result <= 0)
    result += RAND_M;
  return result;
}

static void
init_noise (float *data, int32_t seed)
{
  float s, x, y;
  int i, j, k;

  seed = setup_seed (seed);

  for (i = 0; i < BSize; i++)
    set_lattice (data, i, i);

  for (k = 0; k < 4; k++)
    {
      for (i = 0; i < BSize; i++)
        {
          do
            {
              seed = gsk_random (seed);
              x = (float) ((seed % (BSize + BSize)) - BSize) / BSize;
              seed = gsk_random (seed);
              y = (float) ((seed % (BSize + BSize)) - BSize) / BSize;
            }
          while (x == 0 && y == 0);

          s = sqrt (x * x + y * y);

          x /= s;
          y /= s;

          set_gradient (data, k, i, x, y);
        }
    }

  i = BSize;
  while (--i)
    {
      k = get_lattice (data, i);
      seed = gsk_random (seed);
      j = ((unsigned int) seed) % BSize;
      set_lattice (data, i, get_lattice (data, j));
      set_lattice (data, j, k);
    }

  for (i = 0; i < BSize + 2; i++)
    {
      set_lattice (data, BSize + i, get_lattice (data, i));
      for (k = 0; k < 4; k++)
        {
          const float *g = get_gradient (data, k, i);
          set_gradient (data, k, BSize + i, g[0], g[1]);
        }
    }
}

#define s_curve(t) ((t) * (t) * (3.0 - 2.0 * (t)))
#define lerp(t, a, b) ((a) + (t) * ((b) - (a)))

static double
noise2 (const float *lookup,
        int          channel,
        double       vec[2],
        StitchInfo  *stitch_info)
{
  size_t bx0, bx1, by0, by1, b00, b10, b01, b11;
  double rx0, rx1, ry0, ry1, sx, sy, a, b, u, v;
  double fx, fy;
  int i, j;
  const float *q;

  fx = vec[0] + PerlinN;
  bx0 = (size_t) fx;
  bx1 = bx0 + 1;
  rx0 = fx - floor (fx);
  rx1 = rx0 - 1.0;

  fy = vec[1] + PerlinN;
  by0 = (size_t) fy;
  by1 = by0 + 1;
  ry0 = fy - floor (fy);
  ry1 = ry0 - 1.0;

  if (stitch_info != NULL)
    {
      if (bx0 >= stitch_info->wrap_x)
        bx0 -= stitch_info->width;
      if (bx1 >= stitch_info->wrap_x)
        bx1 -= stitch_info->width;
      if (by0 >= stitch_info->wrap_y)
        by0 -= stitch_info->height;
      if (by1 >= stitch_info->wrap_y)
        by1 -= stitch_info->height;
    }

  bx0 &= BM;
  bx1 &= BM;
  by0 &= BM;
  by1 &= BM;

  i = get_lattice (lookup, bx0);
  j = get_lattice (lookup, bx1);
  b00 = get_lattice (lookup, i + by0);
  b10 = get_lattice (lookup, j + by0);
  b01 = get_lattice (lookup, i + by1);
  b11 = get_lattice (lookup, j + by1);

  sx = s_curve (rx0);
  sy = s_curve (ry0);

  q = get_gradient (lookup, channel, b00);
  u = rx0 * q[0] + ry0 * q[1];
  q = get_gradient (lookup, channel, b10);
  v = rx1 * q[0] + ry0 * q[1];
  a = lerp (sx, u, v);
  q = get_gradient (lookup, channel, b01);
  u = rx0 * q[0] + ry1 * q[1];
  q = get_gradient (lookup, channel, b11);
  v = rx1 * q[0] + ry1 * q[1];
  b = lerp (sx, u, v);

  return lerp (sy, a, b);
}

static double
turbulence (const float           *lookup,
            int                    channel,
            double                 point[2],
            const graphene_size_t *base_freq,
            int                    n_octaves,
            gboolean               is_fractal,
            gboolean               stitch_tiles,
            const graphene_rect_t *bounds)
{
  StitchInfo stitch;
  StitchInfo *stitch_info = NULL;
  float freq_h = base_freq->width;
  float freq_v = base_freq->height;
  double sum, ratio, vec[2];

  if (stitch_tiles)
    {
      if (freq_h != 0.0)
        {
          double freq_low = floor (bounds->size.width * freq_h) / bounds->size.width;
          double freq_high = ceil (bounds->size.width * freq_h) / bounds->size.width;
          if (freq_h / freq_low < freq_high / freq_h)
            freq_h = freq_low;
          else
            freq_h = freq_high;
        }
      if (freq_v != 0.0)
        {
          double freq_low = floor (bounds->size.height * freq_v) / bounds->size.height;
          double freq_high = ceil (bounds->size.height * freq_v) / bounds->size.height;
          if (freq_v / freq_low < freq_high / freq_v)
            freq_v = freq_low;
          else
            freq_v = freq_high;
        }

      stitch.width = (unsigned int) round (bounds->size.width * freq_h);
      stitch.wrap_x = PerlinN + stitch.width;
      stitch.height = (unsigned int) round (bounds->size.height * freq_v);
      stitch.wrap_y = PerlinN + stitch.height;
      stitch_info = &stitch;
    }

  sum = 0;
  vec[0] = point[0] * freq_h;
  vec[1] = point[1] * freq_v;
  ratio = 1;

  for (int octave = 0; octave < n_octaves; octave++)
    {
      if (is_fractal)
        sum += noise2 (lookup, channel, vec, stitch_info) / ratio;
      else
        sum += fabs (noise2 (lookup, channel, vec, stitch_info)) / ratio;

      vec[0] *= 2;
      vec[1] *= 2;
      ratio *= 2;

      if (stitch_info != NULL)
        {
          stitch.width *= 2;
          stitch.wrap_x = 2 * stitch.wrap_x - (unsigned int) PerlinN;
          stitch.height *= 2;
          stitch.wrap_y = 2 * stitch.wrap_y - (unsigned int) PerlinN;
        }
    }

  return sum;
}

static void
gsk_turbulence_node_finalize (GskRenderNode *node)
{
  GskTurbulenceNode *self = (GskTurbulenceNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_TURBULENCE_NODE));

  gdk_color_state_unref (self->color_state);

  parent_class->finalize (node);
}

static void
gsk_turbulence_node_draw (GskRenderNode *node,
                          cairo_t       *cr,
                          GskCairoData  *data)
{
  GskTurbulenceNode *self = (GskTurbulenceNode *) node;
  graphene_rect_t bounds;
  cairo_surface_t *surface;
  uint8_t *pixels;
  int stride;
  int width, height;
  int x, y;
  gboolean is_fractal = (self->noise_type == GSK_NOISE_FRACTAL_NOISE);
  double point[2];
  gboolean cs_equal;

  if (!gsk_cairo_rect_snap (cr, &node->bounds, self->snap, &bounds))
    return;

  width = (int) ceil (bounds.size.width);
  height = (int) ceil (bounds.size.height);

  if (width <= 0 || height <= 0)
    return;

  cs_equal = gdk_color_state_equal (data->ccs, self->color_state);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  pixels = cairo_image_surface_get_data (surface);
  stride = cairo_image_surface_get_stride (surface);

  for (y = 0; y < height; y++)
    {
      uint32_t *row = (uint32_t *) (pixels + y * stride);
      for (x = 0; x < width; x++)
        {
          double noise_r, noise_g, noise_b, noise_a;
          float r, g, b, a;
          GdkColor c, l;

          point[0] = bounds.origin.x + x;
          point[1] = bounds.origin.y + y;

          noise_r = turbulence (self->lookup, 0, point, &self->base_frequency,
                                self->num_octaves, is_fractal, self->stitch_tiles,
                                &bounds);
          noise_g = turbulence (self->lookup, 1, point, &self->base_frequency,
                                self->num_octaves, is_fractal, self->stitch_tiles,
                                &bounds);
          noise_b = turbulence (self->lookup, 2, point, &self->base_frequency,
                                self->num_octaves, is_fractal, self->stitch_tiles,
                                &bounds);
          noise_a = turbulence (self->lookup, 3, point, &self->base_frequency,
                                self->num_octaves, is_fractal, self->stitch_tiles,
                                &bounds);

          if (is_fractal)
            {
              r = CLAMP ((noise_r + 1) / 2, 0, 1);
              g = CLAMP ((noise_g + 1) / 2, 0, 1);
              b = CLAMP ((noise_b + 1) / 2, 0, 1);
              a = CLAMP ((noise_a + 1) / 2, 0, 1);
            }
          else
            {
              r = CLAMP (noise_r, 0, 1);
              g = CLAMP (noise_g, 0, 1);
              b = CLAMP (noise_b, 0, 1);
              a = CLAMP (noise_a, 0, 1);
            }

          if (!cs_equal)
            {
              gdk_color_init (&c, self->color_state, (float[]) { r, g, b, a });
              gdk_color_convert (&l, data->ccs, &c);
              r = l.r;
              g = l.g;
              b = l.g;
              a = l.a;
            }

          r *= a;
          g *= a;
          b *= a;

          row[x] = CLAMP ((int) round (a * 255), 0, 255) << 24 |
                   CLAMP ((int) round (r * 255), 0, 255) << 16 |
                   CLAMP ((int) round (g * 255), 0, 255) << 8 |
                   CLAMP ((int) round (b * 255), 0, 255) << 0;
        }
    }

  cairo_surface_mark_dirty (surface);

  cairo_set_source_surface (cr, surface, bounds.origin.x, bounds.origin.y);
  cairo_paint (cr);

  cairo_surface_destroy (surface);
}

static void
gsk_turbulence_node_diff (GskRenderNode *node1,
                          GskRenderNode *node2,
                          GskDiffData   *data)
{
  GskTurbulenceNode *self1 = (GskTurbulenceNode *) node1;
  GskTurbulenceNode *self2 = (GskTurbulenceNode *) node2;

  if (gsk_rect_equal (&node1->bounds, &node2->bounds) &&
      self1->base_frequency.width == self2->base_frequency.width &&
      self1->base_frequency.height == self2->base_frequency.height &&
      self1->num_octaves == self2->num_octaves &&
      self1->seed == self2->seed &&
      self1->noise_type == self2->noise_type &&
      self1->stitch_tiles == self2->stitch_tiles)
    return;

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode *
gsk_turbulence_node_replay (GskRenderNode   *node,
                            GskRenderReplay *replay)
{
  return gsk_render_node_ref (node);
}

static void
gsk_turbulence_node_class_init (gpointer g_class,
                                gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_TURBULENCE_NODE;

  node_class->finalize = gsk_turbulence_node_finalize;
  node_class->draw = gsk_turbulence_node_draw;
  node_class->diff = gsk_turbulence_node_diff;
  node_class->replay = gsk_turbulence_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskTurbulenceNode, gsk_turbulence_node)

/*< private >
 * gsk_turbulence_node_new:
 * @bounds: The bounds of the node
 * @snap: How to snap the rectangle to the pixel grid
 * @color_state: The color state to return noise in
 * @base_frequency: the base frequencies
 * @num_octaves: The number of octaves of noise
 * @seed: The random seed
 * @noise_type: The type of noise pattern
 * @stitch_tiles: Whether to enable tile stitching
 *
 * Creates a new `GskRenderNode` that generates a noise pattern.
 *
 * The node generates a Perlin noise or turbulence pattern according to the
 * CSS Filter Effects specification.
 *
 * Returns: (transfer full): a new render node
 */
GskRenderNode *
gsk_turbulence_node_new (const graphene_rect_t *bounds,
                         GskRectSnap            snap,
                         GdkColorState         *color_state,
                         const graphene_size_t *base_frequency,
                         unsigned int           num_octaves,
                         int                    seed,
                         GskNoiseType           noise_type,
                         gboolean               stitch_tiles)
{
  GskTurbulenceNode *self;
  GskRenderNode *node;

  self = gsk_render_node_alloc (GSK_TYPE_TURBULENCE_NODE);
  node = (GskRenderNode *) self;

  self->snap = snap;
  self->color_state = gdk_color_state_ref (color_state);

  self->base_frequency = *base_frequency;
  self->num_octaves = num_octaves;
  self->seed = seed;
  self->noise_type = noise_type;
  self->stitch_tiles = stitch_tiles;

  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);

  node->preferred_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
  node->is_hdr = FALSE;
  node->contains_subsurface_node = FALSE;
  node->contains_paste_node = FALSE;

  init_noise (self->lookup, seed);

  return node;
}

/*< private >
 * gsk_turbulence_node_get_color_state:
 * @node: (type GskTurbulenceNode): a `GskRenderNode`
 *
 * Retrieves the color state of the @node.
 *
 * Returns: (transfer none): the color state
 */
GdkColorState *
gsk_turbulence_node_get_color_state (const GskRenderNode *node)
{
  const GskTurbulenceNode *self = (const GskTurbulenceNode *) node;

  return self->color_state;
}

/*< private >
 * gsk_turbulence_node_get_snap:
 * @node: (type GskTurbulenceNode): a `GskRenderNode`
 *
 * Retrieves the snap value for this node
 *
 * Returns: the snap value
 **/
GskRectSnap
gsk_turbulence_node_get_snap (const GskRenderNode *node)
{
  const GskTurbulenceNode *self = (const GskTurbulenceNode *) node;

  return self->snap;
}

/*< private >
 * gsk_turbulence_node_get_base_frequency:
 * @node: (type GskTurbulenceNode): a `GskRenderNode`
 *
 * Returns the base frequencies in the horizontal and
 * vertical direction for the noise pattern.
 *
 * Returns: the frequencies
 */
const graphene_size_t *
gsk_turbulence_node_get_base_frequency (const GskRenderNode *node)
{
  GskTurbulenceNode *self = (GskTurbulenceNode *) node;

  return &self->base_frequency;
}

/*< private >
 * gsk_turbulence_node_get_num_octaves:
 * @node: (type GskTurbulenceNode): a `GskRenderNode`
 *
 * Returns the number of octaves for the noise pattern.
 *
 * Returns: the number of octaves
 */
unsigned int
gsk_turbulence_node_get_num_octaves (const GskRenderNode *node)
{
  GskTurbulenceNode *self = (GskTurbulenceNode *) node;

  return self->num_octaves;
}

/*< private >
 * gsk_turbulence_node_get_seed:
 * @node: (type GskTurbulenceNode): a `GskRenderNode`
 *
 * Returns the gsk_random seed used for the noise pattern.
 *
 * Returns: the gsk_random seed
 */
int
gsk_turbulence_node_get_seed (const GskRenderNode *node)
{
  GskTurbulenceNode *self = (GskTurbulenceNode *) node;

  return self->seed;
}

/*< private >
 * gsk_turbulence_node_get_noise_type:
 * @node: (type GskTurbulenceNode): a `GskRenderNode`
 *
 * Returns the type of noise pattern for this node.
 *
 * Returns: the noise type
 */
GskNoiseType
gsk_turbulence_node_get_noise_type (const GskRenderNode *node)
{
  GskTurbulenceNode *self = (GskTurbulenceNode *) node;

  return self->noise_type;
}

/*< private >
 * gsk_turbulence_node_get_stitch_tiles:
 * @node: (type GskTurbulenceNode): a `GskRenderNode`
 *
 * Returns whether tile stitching is enabled for this node.
 *
 * Returns: %TRUE if stitching is enabled
 */
gboolean
gsk_turbulence_node_get_stitch_tiles (const GskRenderNode *node)
{
  GskTurbulenceNode *self = (GskTurbulenceNode *) node;

  return self->stitch_tiles;
}

const float *
gsk_turbulence_node_get_lookup_table (const GskRenderNode *node)
{
  GskTurbulenceNode *self = (GskTurbulenceNode *) node;

  return self->lookup;
}

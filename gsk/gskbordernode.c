/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
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

#include "gskbordernode.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskroundedrectprivate.h"

#include "gdk/gdkcairoprivate.h"

G_LOCK_DEFINE_STATIC (rgba);

/**
 * GskBorderNode:
 *
 * A render node for a border.
 */
struct _GskBorderNode
{
  GskRenderNode render_node;

  bool uniform_width: 1;
  bool uniform_color: 1;
  GskRoundedRect outline;
  float border_width[4];
  GdkColor border_color[4];
  GdkRGBA *border_rgba;
};

static void
gsk_border_node_finalize (GskRenderNode *node)
{
  GskBorderNode *self = (GskBorderNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_BORDER_NODE));

  for (int i = 0; i < 4; i++)
    gdk_color_finish (&self->border_color[i]);

  g_free (self->border_rgba);

  parent_class->finalize (node);
}

static void
gsk_border_node_mesh_add_patch (cairo_pattern_t *pattern,
                                GdkColorState   *ccs,
                                const GdkColor  *color,
                                double           x0,
                                double           y0,
                                double           x1,
                                double           y1,
                                double           x2,
                                double           y2,
                                double           x3,
                                double           y3)
{
  float values[4];

  gdk_color_to_float (color, ccs, values);

  cairo_mesh_pattern_begin_patch (pattern);
  cairo_mesh_pattern_move_to (pattern, x0, y0);
  cairo_mesh_pattern_line_to (pattern, x1, y1);
  cairo_mesh_pattern_line_to (pattern, x2, y2);
  cairo_mesh_pattern_line_to (pattern, x3, y3);
  cairo_mesh_pattern_set_corner_color_rgba (pattern, 0, values[0], values[1], values[2], values[3]);
  cairo_mesh_pattern_set_corner_color_rgba (pattern, 1, values[0], values[1], values[2], values[3]);
  cairo_mesh_pattern_set_corner_color_rgba (pattern, 2, values[0], values[1], values[2], values[3]);
  cairo_mesh_pattern_set_corner_color_rgba (pattern, 3, values[0], values[1], values[2], values[3]);
  cairo_mesh_pattern_end_patch (pattern);
}

static void
gsk_border_node_draw (GskRenderNode *node,
                       cairo_t       *cr,
                       GdkColorState *ccs)
{
  GskBorderNode *self = (GskBorderNode *) node;
  GskRoundedRect inside;

  cairo_save (cr);

  gsk_rounded_rect_init_copy (&inside, &self->outline);
  gsk_rounded_rect_shrink (&inside,
                           self->border_width[0], self->border_width[1],
                           self->border_width[2], self->border_width[3]);

  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
  gsk_rounded_rect_path (&self->outline, cr);
  gsk_rounded_rect_path (&inside, cr);

  if (gdk_color_equal (&self->border_color[0], &self->border_color[1]) &&
      gdk_color_equal (&self->border_color[0], &self->border_color[2]) &&
      gdk_color_equal (&self->border_color[0], &self->border_color[3]))
    {
      gdk_cairo_set_source_color (cr, ccs, &self->border_color[0]);
    }
  else
    {
      const graphene_rect_t *bounds = &self->outline.bounds;
      /* distance to center "line":
       * +-------------------------+
       * |                         |
       * |                         |
       * |     ---this-line---     |
       * |                         |
       * |                         |
       * +-------------------------+
       * That line is equidistant from all sides. It's either horizontal
       * or vertical, depending on if the rect is wider or taller.
       * We use the 4 sides spanned up by connecting the line to the corner
       * points to color the regions of the rectangle differently.
       * Note that the call to cairo_fill() will add the potential final
       * segment by closing the path, so we don't have to care.
       */
      cairo_pattern_t *mesh;
      cairo_matrix_t mat;
      graphene_point_t tl, br;
      float scale;

      mesh = cairo_pattern_create_mesh ();
      cairo_matrix_init_translate (&mat, -bounds->origin.x, -bounds->origin.y);
      cairo_pattern_set_matrix (mesh, &mat);

      scale = MIN (bounds->size.width / (self->border_width[1] + self->border_width[3]),
                   bounds->size.height / (self->border_width[0] + self->border_width[2]));
      graphene_point_init (&tl,
                           self->border_width[3] * scale,
                           self->border_width[0] * scale);
      graphene_point_init (&br,
                           bounds->size.width - self->border_width[1] * scale,
                           bounds->size.height - self->border_width[2] * scale);

      /* Top */
      if (self->border_width[0] > 0)
        {
          gsk_border_node_mesh_add_patch (mesh,
                                          ccs,
                                          &self->border_color[0],
                                          0, 0,
                                          tl.x, tl.y,
                                          br.x, tl.y,
                                          bounds->size.width, 0);
        }

      /* Right */
      if (self->border_width[1] > 0)
        {
          gsk_border_node_mesh_add_patch (mesh,
                                          ccs,
                                          &self->border_color[1],
                                          bounds->size.width, 0,
                                          br.x, tl.y,
                                          br.x, br.y,
                                          bounds->size.width, bounds->size.height);
        }

      /* Bottom */
      if (self->border_width[2] > 0)
        {
          gsk_border_node_mesh_add_patch (mesh,
                                          ccs,
                                          &self->border_color[2],
                                          0, bounds->size.height,
                                          tl.x, br.y,
                                          br.x, br.y,
                                          bounds->size.width, bounds->size.height);
        }

      /* Left */
      if (self->border_width[3] > 0)
        {
          gsk_border_node_mesh_add_patch (mesh,
                                          ccs,
                                          &self->border_color[3],
                                          0, 0,
                                          tl.x, tl.y,
                                          tl.x, br.y,
                                          0, bounds->size.height);
        }

      cairo_set_source (cr, mesh);
      cairo_pattern_destroy (mesh);
    }

  cairo_fill (cr);
  cairo_restore (cr);
}

static void
gsk_border_node_diff (GskRenderNode *node1,
                      GskRenderNode *node2,
                      GskDiffData   *data)
{
  GskBorderNode *self1 = (GskBorderNode *) node1;
  GskBorderNode *self2 = (GskBorderNode *) node2;
  gboolean uniform1 = self1->uniform_width && self1->uniform_color;
  gboolean uniform2 = self2->uniform_width && self2->uniform_color;

  if (uniform1 &&
      uniform2 &&
      self1->border_width[0] == self2->border_width[0] &&
      gsk_rounded_rect_equal (&self1->outline, &self2->outline) &&
      gdk_color_equal (&self1->border_color[0], &self2->border_color[0]))
    return;

  /* Different uniformity -> diff impossible */
  if (uniform1 ^ uniform2)
    {
      gsk_render_node_diff_impossible (node1, node2, data);
      return;
    }

  if (self1->border_width[0] == self2->border_width[0] &&
      self1->border_width[1] == self2->border_width[1] &&
      self1->border_width[2] == self2->border_width[2] &&
      self1->border_width[3] == self2->border_width[3] &&
      gdk_color_equal (&self1->border_color[0], &self2->border_color[0]) &&
      gdk_color_equal (&self1->border_color[1], &self2->border_color[1]) &&
      gdk_color_equal (&self1->border_color[2], &self2->border_color[2]) &&
      gdk_color_equal (&self1->border_color[3], &self2->border_color[3]) &&
      gsk_rounded_rect_equal (&self1->outline, &self2->outline))
    return;

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode *
gsk_border_node_replay (GskRenderNode   *node,
                        GskRenderReplay *replay)
{
  return gsk_render_node_ref (node);
}

static void
gsk_border_node_class_init (gpointer g_class,
                            gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_BORDER_NODE;

  node_class->finalize = gsk_border_node_finalize;
  node_class->draw = gsk_border_node_draw;
  node_class->diff = gsk_border_node_diff;
  node_class->replay = gsk_border_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskBorderNode, gsk_border_node)

/**
 * gsk_border_node_get_outline:
 * @node: (type GskBorderNode): a `GskRenderNode` for a border
 *
 * Retrieves the outline of the border.
 *
 * Returns: the outline of the border
 */
const GskRoundedRect *
gsk_border_node_get_outline (const GskRenderNode *node)
{
  const GskBorderNode *self = (const GskBorderNode *) node;

  return &self->outline;
}

/**
 * gsk_border_node_get_widths:
 * @node: (type GskBorderNode): a `GskRenderNode` for a border
 *
 * Retrieves the stroke widths of the border.
 *
 * Returns: (transfer none) (array fixed-size=4): an array of 4 floats
 *   for the top, right, bottom and left stroke width of the border,
 *   respectively
 */
const float *
gsk_border_node_get_widths (const GskRenderNode *node)
{
  const GskBorderNode *self = (const GskBorderNode *) node;

  return self->border_width;
}

/**
 * gsk_border_node_get_colors:
 * @node: (type GskBorderNode): a `GskRenderNode` for a border
 *
 * Retrieves the colors of the border.
 *
 * Returns: (transfer none) (array fixed-size=4): an array of 4 `GdkRGBA`
 *   structs for the top, right, bottom and left color of the border
 */
const GdkRGBA *
gsk_border_node_get_colors (const GskRenderNode *node)
{
  GskBorderNode *self = (GskBorderNode *) node;
  const GdkRGBA *colors;

  G_LOCK (rgba);

  if (self->border_rgba == NULL)
    {
      self->border_rgba = g_new (GdkRGBA, 4);
      for (int i = 0; i < 4; i++)
        gdk_color_to_float (&self->border_color[i], GDK_COLOR_STATE_SRGB, (float *) &self->border_rgba[i]);
    }

  colors = self->border_rgba;

  G_UNLOCK (rgba);

  return colors;
}

/**
 * gsk_border_node_new:
 * @outline: a `GskRoundedRect` describing the outline of the border
 * @border_width: (array fixed-size=4): the stroke width of the border on
 *     the top, right, bottom and left side respectively.
 * @border_color: (array fixed-size=4): the color used on the top, right,
 *     bottom and left side.
 *
 * Creates a `GskRenderNode` that will stroke a border rectangle inside the
 * given @outline.
 *
 * The 4 sides of the border can have different widths and colors.
 *
 * Returns: (transfer full) (type GskBorderNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_border_node_new (const GskRoundedRect *outline,
                     const float           border_width[4],
                     const GdkRGBA         border_color[4])
{
  GskRenderNode *node;
  GdkColor color[4];

  for (int i = 0; i < 4; i++)
    gdk_color_init_from_rgba (&color[i], &border_color[i]);

  node = gsk_border_node_new2 (outline, border_width, color);

  for (int i = 0; i < 4; i++)
    gdk_color_finish (&color[i]);

  return node;
}

/*< private >
 * gsk_border_node_new2:
 * @outline: a `GskRoundedRect` describing the outline of the border
 * @border_width: (array fixed-size=4): the stroke width of the border on
 *     the top, right, bottom and left side respectively.
 * @border_color: (array fixed-size=4): the color used on the top, right,
 *     bottom and left side.
 *
 * Creates a `GskRenderNode` that will stroke a border rectangle inside the
 * given @outline.
 *
 * The 4 sides of the border can have different widths and colors.
 *
 * Returns: (transfer full) (type GskBorderNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_border_node_new2 (const GskRoundedRect *outline,
                      const float           border_width[4],
                      const GdkColor        border_color[4])
{
  GskBorderNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (border_width != NULL, NULL);
  g_return_val_if_fail (border_color != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_BORDER_NODE);
  node = (GskRenderNode *) self;
  node->preferred_depth = GDK_MEMORY_NONE;

  gsk_rounded_rect_init_copy (&self->outline, outline);
  memcpy (self->border_width, border_width, sizeof (self->border_width));
  memcpy (self->border_color, border_color, sizeof (self->border_color));
  for (int i = 0; i < 4; i++)
    gdk_color_init_copy (&self->border_color[i], &border_color[i]);

  if (border_width[0] == border_width[1] &&
      border_width[0] == border_width[2] &&
      border_width[0] == border_width[3])
    self->uniform_width = TRUE;
  else
    self->uniform_width = FALSE;

  if (gdk_color_equal (&border_color[0], &border_color[1]) &&
      gdk_color_equal (&border_color[0], &border_color[2]) &&
      gdk_color_equal (&border_color[0], &border_color[3]))
    self->uniform_color = TRUE;
  else
    self->uniform_color = FALSE;

  gsk_rect_init_from_rect (&node->bounds, &self->outline.bounds);

  return node;
}

/*< private >
 * gsk_border_node_get_gdk_colors:
 * @node: (type GskBorderNode): a `GskRenderNode` for a border
 *
 * Retrieves the colors of the border.
 *
 * Returns: (transfer none) (array fixed-size=4): an array of 4 `GdkColor`
 *   structs for the top, right, bottom and left color of the border
 */
const GdkColor *
gsk_border_node_get_gdk_colors (const GskRenderNode *node)
{
  const GskBorderNode *self = (const GskBorderNode *) node;

  return self->border_color;
}

bool
gsk_border_node_get_uniform (const GskRenderNode *self)
{
  const GskBorderNode *node = (const GskBorderNode *)self;
  return node->uniform_width && node->uniform_color;
}

bool
gsk_border_node_get_uniform_color (const GskRenderNode *self)
{
  const GskBorderNode *node = (const GskBorderNode *)self;
  return node->uniform_color;
}

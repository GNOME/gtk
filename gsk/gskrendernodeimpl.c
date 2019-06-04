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

#include "gskrendernodeprivate.h"

#include "gskcairoblurprivate.h"
#include "gskdebugprivate.h"
#include "gskdiffprivate.h"
#include "gskrendererprivate.h"
#include "gskroundedrectprivate.h"
#include "gsktransformprivate.h"

#include "gdk/gdktextureprivate.h"

static void
rectangle_init_from_graphene (cairo_rectangle_int_t *cairo,
                              const graphene_rect_t *graphene)
{
  cairo->x = floorf (graphene->origin.x);
  cairo->y = floorf (graphene->origin.y);
  cairo->width = ceilf (graphene->origin.x + graphene->size.width) - cairo->x;
  cairo->height = ceilf (graphene->origin.y + graphene->size.height) - cairo->y;
}

static gboolean
gsk_render_node_can_diff_true (GskRenderNode *node1,
                               GskRenderNode *node2)
{
  return TRUE;
}

/*** GSK_COLOR_NODE ***/

typedef struct _GskColorNode GskColorNode;

struct _GskColorNode
{
  GskRenderNode render_node;

  GdkRGBA color;
};

static void
gsk_color_node_finalize (GskRenderNode *node)
{
}

static void
gsk_color_node_draw (GskRenderNode *node,
                     cairo_t       *cr)
{
  GskColorNode *self = (GskColorNode *) node;

  gdk_cairo_set_source_rgba (cr, &self->color);

  cairo_rectangle (cr,
                   node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_fill (cr);
}

static void
gsk_color_node_diff (GskRenderNode  *node1,
                     GskRenderNode  *node2,
                     cairo_region_t *region)
{
  GskColorNode *self1 = (GskColorNode *) node1;
  GskColorNode *self2 = (GskColorNode *) node2;

  if (graphene_rect_equal (&node1->bounds, &node2->bounds) &&
      gdk_rgba_equal (&self1->color, &self2->color))
    return;

  gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_COLOR_NODE_CLASS = {
  GSK_COLOR_NODE,
  sizeof (GskColorNode),
  "GskColorNode",
  gsk_color_node_finalize,
  gsk_color_node_draw,
  gsk_render_node_can_diff_true,
  gsk_color_node_diff,
};

const GdkRGBA *
gsk_color_node_peek_color (GskRenderNode *node)
{
  GskColorNode *self = (GskColorNode *) node;

  return &self->color;
}

/**
 * gsk_color_node_new:
 * @rgba: a #GdkRGBA specifying a color
 * @bounds: the rectangle to render the color into
 *
 * Creates a #GskRenderNode that will render the color specified by @rgba into
 * the area given by @bounds.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_color_node_new (const GdkRGBA         *rgba,
                    const graphene_rect_t *bounds)
{
  GskColorNode *self;

  g_return_val_if_fail (rgba != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  self = (GskColorNode *) gsk_render_node_new (&GSK_COLOR_NODE_CLASS, 0);

  self->color = *rgba;
  graphene_rect_init_from_rect (&self->render_node.bounds, bounds);

  return &self->render_node;
}

/*** GSK_LINEAR_GRADIENT_NODE ***/

typedef struct _GskLinearGradientNode GskLinearGradientNode;

struct _GskLinearGradientNode
{
  GskRenderNode render_node;

  graphene_point_t start;
  graphene_point_t end;

  gsize n_stops;
  GskColorStop stops[];
};

static void
gsk_linear_gradient_node_finalize (GskRenderNode *node)
{
}

static void
gsk_linear_gradient_node_draw (GskRenderNode *node,
                               cairo_t       *cr)
{
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;
  cairo_pattern_t *pattern;
  gsize i;

  pattern = cairo_pattern_create_linear (self->start.x, self->start.y,
                                         self->end.x, self->end.y);

  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE)
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

  for (i = 0; i < self->n_stops; i++)
    {
      cairo_pattern_add_color_stop_rgba (pattern,
                                         self->stops[i].offset,
                                         self->stops[i].color.red,
                                         self->stops[i].color.green,
                                         self->stops[i].color.blue,
                                         self->stops[i].color.alpha);
    }

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  cairo_rectangle (cr,
                   node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_fill (cr);
}

static void
gsk_linear_gradient_node_diff (GskRenderNode  *node1,
                               GskRenderNode  *node2,
                               cairo_region_t *region)
{
  GskLinearGradientNode *self1 = (GskLinearGradientNode *) node1;
  GskLinearGradientNode *self2 = (GskLinearGradientNode *) node2;

  if (graphene_point_equal (&self1->start, &self2->start) &&
      graphene_point_equal (&self1->end, &self2->end) &&
      self1->n_stops == self2->n_stops)
    {
      gsize i;

      for (i = 0; i < self1->n_stops; i++)
        {
          GskColorStop *stop1 = &self1->stops[i];
          GskColorStop *stop2 = &self2->stops[i];

          if (stop1->offset == stop2->offset &&
              gdk_rgba_equal (&stop1->color, &stop2->color))
            continue;

          gsk_render_node_diff_impossible (node1, node2, region);
          return;
        }

      return;
    }
  
  gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_LINEAR_GRADIENT_NODE_CLASS = {
  GSK_LINEAR_GRADIENT_NODE,
  sizeof (GskLinearGradientNode),
  "GskLinearGradientNode",
  gsk_linear_gradient_node_finalize,
  gsk_linear_gradient_node_draw,
  gsk_render_node_can_diff_true,
  gsk_linear_gradient_node_diff,
};

static const GskRenderNodeClass GSK_REPEATING_LINEAR_GRADIENT_NODE_CLASS = {
  GSK_REPEATING_LINEAR_GRADIENT_NODE,
  sizeof (GskLinearGradientNode),
  "GskRepeatingLinearGradientNode",
  gsk_linear_gradient_node_finalize,
  gsk_linear_gradient_node_draw,
  gsk_render_node_can_diff_true,
  gsk_linear_gradient_node_diff,
};

/**
 * gsk_linear_gradient_node_new:
 * @bounds: the rectangle to render the linear gradient into
 * @start: the point at which the linear gradient will begin
 * @end: the point at which the linear gradient will finish
 * @color_stops: (array length=n_color_stops): a pointer to an array of #GskColorStop defining the gradient
 * @n_color_stops: the number of elements in @color_stops
 *
 * Creates a #GskRenderNode that will create a linear gradient from the given
 * points and color stops, and render that into the area given by @bounds.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_linear_gradient_node_new (const graphene_rect_t  *bounds,
                              const graphene_point_t *start,
                              const graphene_point_t *end,
                              const GskColorStop     *color_stops,
                              gsize                   n_color_stops)
{
  GskLinearGradientNode *self;
  gsize i;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);
  g_return_val_if_fail (color_stops[0].offset >= 0, NULL);
  for (i = 1; i < n_color_stops; i++)
    g_return_val_if_fail (color_stops[i].offset >= color_stops[i-1].offset, NULL);
  g_return_val_if_fail (color_stops[n_color_stops - 1].offset <= 1, NULL);

  self = (GskLinearGradientNode *) gsk_render_node_new (&GSK_LINEAR_GRADIENT_NODE_CLASS, sizeof (GskColorStop) * n_color_stops);

  graphene_rect_init_from_rect (&self->render_node.bounds, bounds);
  graphene_point_init_from_point (&self->start, start);
  graphene_point_init_from_point (&self->end, end);

  memcpy (&self->stops, color_stops, sizeof (GskColorStop) * n_color_stops);
  self->n_stops = n_color_stops;

  return &self->render_node;
}

/**
 * gsk_repeating_linear_gradient_node_new:
 * @bounds: the rectangle to render the linear gradient into
 * @start: the point at which the linear gradient will begin
 * @end: the point at which the linear gradient will finish
 * @color_stops: (array length=n_color_stops): a pointer to an array of #GskColorStop defining the gradient
 * @n_color_stops: the number of elements in @color_stops
 *
 * Creates a #GskRenderNode that will create a repeating linear gradient
 * from the given points and color stops, and render that into the area
 * given by @bounds.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_repeating_linear_gradient_node_new (const graphene_rect_t  *bounds,
                                        const graphene_point_t *start,
                                        const graphene_point_t *end,
                                        const GskColorStop     *color_stops,
                                        gsize                   n_color_stops)
{
  GskLinearGradientNode *self;
  gsize i;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);
  g_return_val_if_fail (color_stops[0].offset >= 0, NULL);
  for (i = 1; i < n_color_stops; i++)
    g_return_val_if_fail (color_stops[i].offset >= color_stops[i-1].offset, NULL);
  g_return_val_if_fail (color_stops[n_color_stops - 1].offset <= 1, NULL);

  self = (GskLinearGradientNode *) gsk_render_node_new (&GSK_REPEATING_LINEAR_GRADIENT_NODE_CLASS, sizeof (GskColorStop) * n_color_stops);

  graphene_rect_init_from_rect (&self->render_node.bounds, bounds);
  graphene_point_init_from_point (&self->start, start);
  graphene_point_init_from_point (&self->end, end);

  memcpy (&self->stops, color_stops, sizeof (GskColorStop) * n_color_stops);
  self->n_stops = n_color_stops;

  return &self->render_node;
}

const graphene_point_t *
gsk_linear_gradient_node_peek_start (GskRenderNode *node)
{
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;

  return &self->start;
}

const graphene_point_t *
gsk_linear_gradient_node_peek_end (GskRenderNode *node)
{
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;

  return &self->end;
}

gsize
gsk_linear_gradient_node_get_n_color_stops (GskRenderNode *node)
{
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;

  return self->n_stops;
}

const GskColorStop *
gsk_linear_gradient_node_peek_color_stops (GskRenderNode *node)
{
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;

  return self->stops;
}

/*** GSK_BORDER_NODE ***/

typedef struct _GskBorderNode GskBorderNode;

struct _GskBorderNode
{
  GskRenderNode render_node;

  GskRoundedRect outline;
  float border_width[4];
  GdkRGBA border_color[4];
};

static void
gsk_border_node_finalize (GskRenderNode *node)
{
}

static void
gsk_border_node_mesh_add_patch (cairo_pattern_t *pattern,
                                const GdkRGBA   *color,
                                double           x0,
                                double           y0,
                                double           x1,
                                double           y1,
                                double           x2,
                                double           y2,
                                double           x3,
                                double           y3)
{
  cairo_mesh_pattern_begin_patch (pattern);
  cairo_mesh_pattern_move_to (pattern, x0, y0);
  cairo_mesh_pattern_line_to (pattern, x1, y1);
  cairo_mesh_pattern_line_to (pattern, x2, y2);
  cairo_mesh_pattern_line_to (pattern, x3, y3);
  cairo_mesh_pattern_set_corner_color_rgba (pattern, 0, color->red, color->green, color->blue, color->alpha);
  cairo_mesh_pattern_set_corner_color_rgba (pattern, 1, color->red, color->green, color->blue, color->alpha);
  cairo_mesh_pattern_set_corner_color_rgba (pattern, 2, color->red, color->green, color->blue, color->alpha);
  cairo_mesh_pattern_set_corner_color_rgba (pattern, 3, color->red, color->green, color->blue, color->alpha);
  cairo_mesh_pattern_end_patch (pattern);
}

static void
gsk_border_node_draw (GskRenderNode *node,
                       cairo_t       *cr)
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

  if (gdk_rgba_equal (&self->border_color[0], &self->border_color[1]) &&
      gdk_rgba_equal (&self->border_color[0], &self->border_color[2]) &&
      gdk_rgba_equal (&self->border_color[0], &self->border_color[3]))
    {
      gdk_cairo_set_source_rgba (cr, &self->border_color[0]);
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
       * That line is equidistant from all sides. It's either horiontal
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
gsk_border_node_diff (GskRenderNode  *node1,
                      GskRenderNode  *node2,
                      cairo_region_t *region)
{
  GskBorderNode *self1 = (GskBorderNode *) node1;
  GskBorderNode *self2 = (GskBorderNode *) node2;

  if (gsk_rounded_rect_equal (&self1->outline, &self2->outline) &&
      gdk_rgba_equal (&self1->border_color[0], &self2->border_color[0]) &&
      gdk_rgba_equal (&self1->border_color[1], &self2->border_color[1]) &&
      gdk_rgba_equal (&self1->border_color[2], &self2->border_color[2]) &&
      gdk_rgba_equal (&self1->border_color[3], &self2->border_color[3]) &&
      self1->border_width[0] == self2->border_width[0] &&
      self1->border_width[1] == self2->border_width[1] &&
      self1->border_width[2] == self2->border_width[2] &&
      self1->border_width[3] == self2->border_width[3])
    return;

  gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_BORDER_NODE_CLASS = {
  GSK_BORDER_NODE,
  sizeof (GskBorderNode),
  "GskBorderNode",
  gsk_border_node_finalize,
  gsk_border_node_draw,
  gsk_render_node_can_diff_true,
  gsk_border_node_diff,
};

const GskRoundedRect *
gsk_border_node_peek_outline (GskRenderNode *node)
{
  GskBorderNode *self = (GskBorderNode *) node;

  return &self->outline;
}

const float *
gsk_border_node_peek_widths (GskRenderNode *node)
{
  GskBorderNode *self = (GskBorderNode *) node;

  return self->border_width;
}

const GdkRGBA *
gsk_border_node_peek_colors (GskRenderNode *node)
{
  GskBorderNode *self = (GskBorderNode *) node;

  return self->border_color;
}

/**
 * gsk_border_node_new:
 * @outline: a #GskRoundedRect describing the outline of the border
 * @border_width: (array fixed-size=4): the stroke width of the border on
 *     the top, right, bottom and left side respectively.
 * @border_color: (array fixed-size=4): the color used on the top, right,
 *     bottom and left side.
 *
 * Creates a #GskRenderNode that will stroke a border rectangle inside the
 * given @outline. The 4 sides of the border can have different widths and
 * colors.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_border_node_new (const GskRoundedRect     *outline,
                     const float               border_width[4],
                     const GdkRGBA             border_color[4])
{
  GskBorderNode *self;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (border_width != NULL, NULL);
  g_return_val_if_fail (border_color != NULL, NULL);

  self = (GskBorderNode *) gsk_render_node_new (&GSK_BORDER_NODE_CLASS, 0);

  gsk_rounded_rect_init_copy (&self->outline, outline);
  memcpy (self->border_width, border_width, sizeof (self->border_width));
  memcpy (self->border_color, border_color, sizeof (self->border_color));

  graphene_rect_init_from_rect (&self->render_node.bounds, &self->outline.bounds);

  return &self->render_node;
}

/*** GSK_TEXTURE_NODE ***/

typedef struct _GskTextureNode GskTextureNode;

struct _GskTextureNode
{
  GskRenderNode render_node;

  GdkTexture *texture;
};

static void
gsk_texture_node_finalize (GskRenderNode *node)
{
  GskTextureNode *self = (GskTextureNode *) node;

  g_object_unref (self->texture);
}

static void
gsk_texture_node_draw (GskRenderNode *node,
                       cairo_t       *cr)
{
  GskTextureNode *self = (GskTextureNode *) node;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;

  surface = gdk_texture_download_surface (self->texture);

  cairo_save (cr);

  cairo_translate (cr, node->bounds.origin.x, node->bounds.origin.y);
  cairo_scale (cr,
               node->bounds.size.width / gdk_texture_get_width (self->texture),
               node->bounds.size.height / gdk_texture_get_height (self->texture));

  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);
  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_restore (cr);

  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);
}

static void
gsk_texture_node_diff (GskRenderNode  *node1,
                       GskRenderNode  *node2,
                       cairo_region_t *region)
{
  GskTextureNode *self1 = (GskTextureNode *) node1;
  GskTextureNode *self2 = (GskTextureNode *) node2;

  if (graphene_rect_equal (&node1->bounds, &node2->bounds) &&
      self1->texture == self2->texture)
    return;

  gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_TEXTURE_NODE_CLASS = {
  GSK_TEXTURE_NODE,
  sizeof (GskTextureNode),
  "GskTextureNode",
  gsk_texture_node_finalize,
  gsk_texture_node_draw,
  gsk_render_node_can_diff_true,
  gsk_texture_node_diff,
};

/**
 * gsk_texture_node_get_texture:
 * @node: a #GskRenderNode
 *
 * Returns: (transfer none): the #GdkTexture
 */
GdkTexture *
gsk_texture_node_get_texture (GskRenderNode *node)
{
  GskTextureNode *self = (GskTextureNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TEXTURE_NODE), 0);

  return self->texture;
}

/**
 * gsk_texture_node_new:
 * @texture: the #GdkTexture
 * @bounds: the rectangle to render the texture into
 *
 * Creates a #GskRenderNode that will render the given
 * @texture into the area given by @bounds.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_texture_node_new (GdkTexture            *texture,
                      const graphene_rect_t *bounds)
{
  GskTextureNode *self;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  self = (GskTextureNode *) gsk_render_node_new (&GSK_TEXTURE_NODE_CLASS, 0);

  self->texture = g_object_ref (texture);
  graphene_rect_init_from_rect (&self->render_node.bounds, bounds);

  return &self->render_node;
}

/*** GSK_INSET_SHADOW_NODE ***/

typedef struct _GskInsetShadowNode GskInsetShadowNode;

struct _GskInsetShadowNode
{
  GskRenderNode render_node;

  GskRoundedRect outline;
  GdkRGBA color;
  float dx;
  float dy;
  float spread;
  float blur_radius;
};

static void
gsk_inset_shadow_node_finalize (GskRenderNode *node)
{
}

static gboolean
has_empty_clip (cairo_t *cr)
{
  double x1, y1, x2, y2;

  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
  return x1 == x2 && y1 == y2;
}

static void
draw_shadow (cairo_t             *cr,
             gboolean             inset,
             const GskRoundedRect*box,
             const GskRoundedRect*clip_box,
             float                radius,
             const GdkRGBA       *color,
	     GskBlurFlags         blur_flags)
{
  cairo_t *shadow_cr;
  gboolean do_blur;

  if (has_empty_clip (cr))
    return;

  gdk_cairo_set_source_rgba (cr, color);
  do_blur = (blur_flags & (GSK_BLUR_X | GSK_BLUR_Y)) != 0;
  if (do_blur)
    shadow_cr = gsk_cairo_blur_start_drawing (cr, radius, blur_flags);
  else
    shadow_cr = cr;

  cairo_set_fill_rule (shadow_cr, CAIRO_FILL_RULE_EVEN_ODD);
  gsk_rounded_rect_path (box, shadow_cr);
  if (inset)
    cairo_rectangle (shadow_cr,
                     clip_box->bounds.origin.x, clip_box->bounds.origin.y,
                     clip_box->bounds.size.width, clip_box->bounds.size.height);

  cairo_fill (shadow_cr);

  if (do_blur)
    gsk_cairo_blur_finish_drawing (shadow_cr, radius, color, blur_flags);
}

typedef struct {
  float radius;
  graphene_size_t corner;
} CornerMask;

typedef enum {
  TOP,
  RIGHT,
  BOTTOM,
  LEFT
} Side;

static guint
corner_mask_hash (CornerMask *mask)
{
  return ((guint)mask->radius << 24) ^
    ((guint)(mask->corner.width*4)) << 12 ^
    ((guint)(mask->corner.height*4)) << 0;
}

static gboolean
corner_mask_equal (CornerMask *mask1,
                   CornerMask *mask2)
{
  return
    mask1->radius == mask2->radius &&
    mask1->corner.width == mask2->corner.width &&
    mask1->corner.height == mask2->corner.height;
}

static void
draw_shadow_corner (cairo_t               *cr,
                    gboolean               inset,
                    const GskRoundedRect  *box,
                    const GskRoundedRect  *clip_box,
                    float                  radius,
                    const GdkRGBA         *color,
                    GskCorner              corner,
                    cairo_rectangle_int_t *drawn_rect)
{
  float clip_radius;
  int x1, x2, x3, y1, y2, y3, x, y;
  GskRoundedRect corner_box;
  cairo_t *mask_cr;
  cairo_surface_t *mask;
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;
  float sx, sy;
  static GHashTable *corner_mask_cache = NULL;
  float max_other;
  CornerMask key;
  gboolean overlapped;

  clip_radius = gsk_cairo_blur_compute_pixels (radius);

  overlapped = FALSE;
  if (corner == GSK_CORNER_TOP_LEFT || corner == GSK_CORNER_BOTTOM_LEFT)
    {
      x1 = floor (box->bounds.origin.x - clip_radius);
      x2 = ceil (box->bounds.origin.x + box->corner[corner].width + clip_radius);
      x = x1;
      sx = 1;
      max_other = MAX(box->corner[GSK_CORNER_TOP_RIGHT].width, box->corner[GSK_CORNER_BOTTOM_RIGHT].width);
      x3 = floor (box->bounds.origin.x + box->bounds.size.width - max_other - clip_radius);
      if (x2 > x3)
        overlapped = TRUE;
    }
  else
    {
      x1 = floor (box->bounds.origin.x + box->bounds.size.width - box->corner[corner].width - clip_radius);
      x2 = ceil (box->bounds.origin.x + box->bounds.size.width + clip_radius);
      x = x2;
      sx = -1;
      max_other = MAX(box->corner[GSK_CORNER_TOP_LEFT].width, box->corner[GSK_CORNER_BOTTOM_LEFT].width);
      x3 = ceil (box->bounds.origin.x + max_other + clip_radius);
      if (x3 > x1)
        overlapped = TRUE;
    }

  if (corner == GSK_CORNER_TOP_LEFT || corner == GSK_CORNER_TOP_RIGHT)
    {
      y1 = floor (box->bounds.origin.y - clip_radius);
      y2 = ceil (box->bounds.origin.y + box->corner[corner].height + clip_radius);
      y = y1;
      sy = 1;
      max_other = MAX(box->corner[GSK_CORNER_BOTTOM_LEFT].height, box->corner[GSK_CORNER_BOTTOM_RIGHT].height);
      y3 = floor (box->bounds.origin.y + box->bounds.size.height - max_other - clip_radius);
      if (y2 > y3)
        overlapped = TRUE;
    }
  else
    {
      y1 = floor (box->bounds.origin.y + box->bounds.size.height - box->corner[corner].height - clip_radius);
      y2 = ceil (box->bounds.origin.y + box->bounds.size.height + clip_radius);
      y = y2;
      sy = -1;
      max_other = MAX(box->corner[GSK_CORNER_TOP_LEFT].height, box->corner[GSK_CORNER_TOP_RIGHT].height);
      y3 = ceil (box->bounds.origin.y + max_other + clip_radius);
      if (y3 > y1)
        overlapped = TRUE;
    }

  drawn_rect->x = x1;
  drawn_rect->y = y1;
  drawn_rect->width = x2 - x1;
  drawn_rect->height = y2 - y1;

  cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
  cairo_clip (cr);

  if (inset || overlapped)
    {
      /* Fall back to generic path if inset or if the corner radius
         runs into each other */
      draw_shadow (cr, inset, box, clip_box, radius, color, GSK_BLUR_X | GSK_BLUR_Y);
      return;
    }

  if (has_empty_clip (cr))
    return;

  /* At this point we're drawing a blurred outset corner. The only
   * things that affect the output of the blurred mask in this case
   * is:
   *
   * What corner this is, which defines the orientation (sx,sy)
   * and position (x,y)
   *
   * The blur radius (which also defines the clip_radius)
   *
   * The the horizontal and vertical corner radius
   *
   * We apply the first position and orientation when drawing the
   * mask, so we cache rendered masks based on the blur radius and the
   * corner radius.
   */
  if (corner_mask_cache == NULL)
    corner_mask_cache = g_hash_table_new_full ((GHashFunc)corner_mask_hash,
                                               (GEqualFunc)corner_mask_equal,
                                               g_free, (GDestroyNotify)cairo_surface_destroy);

  key.radius = radius;
  key.corner = box->corner[corner];

  mask = g_hash_table_lookup (corner_mask_cache, &key);
  if (mask == NULL)
    {
      mask = cairo_surface_create_similar_image (cairo_get_target (cr), CAIRO_FORMAT_A8,
                                                 drawn_rect->width + clip_radius,
                                                 drawn_rect->height + clip_radius);
      mask_cr = cairo_create (mask);
      gsk_rounded_rect_init_from_rect (&corner_box, &GRAPHENE_RECT_INIT (clip_radius, clip_radius, 2*drawn_rect->width, 2*drawn_rect->height), 0);
      corner_box.corner[0] = box->corner[corner];
      gsk_rounded_rect_path (&corner_box, mask_cr);
      cairo_fill (mask_cr);
      gsk_cairo_blur_surface (mask, radius, GSK_BLUR_X | GSK_BLUR_Y);
      cairo_destroy (mask_cr);
      g_hash_table_insert (corner_mask_cache, g_memdup (&key, sizeof (key)), mask);
    }

  gdk_cairo_set_source_rgba (cr, color);
  pattern = cairo_pattern_create_for_surface (mask);
  cairo_matrix_init_identity (&matrix);
  cairo_matrix_scale (&matrix, sx, sy);
  cairo_matrix_translate (&matrix, -x, -y);
  cairo_pattern_set_matrix (pattern, &matrix);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
}

static void
draw_shadow_side (cairo_t               *cr,
                  gboolean               inset,
                  const GskRoundedRect  *box,
                  const GskRoundedRect  *clip_box,
                  float                  radius,
                  const GdkRGBA         *color,
                  Side                   side,
                  cairo_rectangle_int_t *drawn_rect)
{
  GskBlurFlags blur_flags = GSK_BLUR_REPEAT;
  gdouble clip_radius;
  int x1, x2, y1, y2;

  clip_radius = gsk_cairo_blur_compute_pixels (radius);

  if (side == TOP || side == BOTTOM)
    {
      blur_flags |= GSK_BLUR_Y;
      x1 = floor (box->bounds.origin.x - clip_radius);
      x2 = ceil (box->bounds.origin.x + box->bounds.size.width + clip_radius);
    }
  else if (side == LEFT)
    {
      x1 = floor (box->bounds.origin.x -clip_radius);
      x2 = ceil (box->bounds.origin.x + clip_radius);
    }
  else
    {
      x1 = floor (box->bounds.origin.x + box->bounds.size.width -clip_radius);
      x2 = ceil (box->bounds.origin.x + box->bounds.size.width + clip_radius);
    }

  if (side == LEFT || side == RIGHT)
    {
      blur_flags |= GSK_BLUR_X;
      y1 = floor (box->bounds.origin.y - clip_radius);
      y2 = ceil (box->bounds.origin.y + box->bounds.size.height + clip_radius);
    }
  else if (side == TOP)
    {
      y1 = floor (box->bounds.origin.y -clip_radius);
      y2 = ceil (box->bounds.origin.y + clip_radius);
    }
  else
    {
      y1 = floor (box->bounds.origin.y + box->bounds.size.height -clip_radius);
      y2 = ceil (box->bounds.origin.y + box->bounds.size.height + clip_radius);
    }

  drawn_rect->x = x1;
  drawn_rect->y = y1;
  drawn_rect->width = x2 - x1;
  drawn_rect->height = y2 - y1;

  cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
  cairo_clip (cr);
  draw_shadow (cr, inset, box, clip_box, radius, color, blur_flags);
}

static gboolean
needs_blur (double radius)
{
  /* The code doesn't actually do any blurring for radius 1, as it
   * ends up with box filter size 1 */
  if (radius <= 1.0)
    return FALSE;

  return TRUE;
}

static void
gsk_inset_shadow_node_draw (GskRenderNode *node,
                            cairo_t       *cr)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;
  GskRoundedRect box, clip_box;
  int clip_radius;
  double x1c, y1c, x2c, y2c;

  /* We don't need to draw invisible shadows */
  if (gdk_rgba_is_clear (&self->color))
    return;

  cairo_clip_extents (cr, &x1c, &y1c, &x2c, &y2c);
  if (!gsk_rounded_rect_intersects_rect (&self->outline, &GRAPHENE_RECT_INIT (x1c, y1c, x2c - x1c, y2c - y1c)))
    return;

  clip_radius = gsk_cairo_blur_compute_pixels (self->blur_radius);

  cairo_save (cr);

  gsk_rounded_rect_path (&self->outline, cr);
  cairo_clip (cr);

  gsk_rounded_rect_init_copy (&box, &self->outline);
  gsk_rounded_rect_offset (&box, self->dx, self->dy);
  gsk_rounded_rect_shrink (&box, self->spread, self->spread, self->spread, self->spread);

  gsk_rounded_rect_init_copy (&clip_box, &self->outline);
  gsk_rounded_rect_shrink (&clip_box, -clip_radius, -clip_radius, -clip_radius, -clip_radius);

  if (!needs_blur (self->blur_radius))
    draw_shadow (cr, TRUE, &box, &clip_box, self->blur_radius, &self->color, GSK_BLUR_NONE);
  else
    {
      cairo_region_t *remaining;
      cairo_rectangle_int_t r;
      int i;

      /* For the blurred case we divide the rendering into 9 parts,
       * 4 of the corners, 4 for the horizonat/vertical lines and
       * one for the interior. We make the non-interior parts
       * large enought to fit the full radius of the blur, so that
       * the interior part can be drawn solidly.
       */

      /* In the inset case we want to paint the whole clip-box.
       * We could remove the part of "box" where the blur doesn't
       * reach, but computing that is a bit tricky since the
       * rounded corners are on the "inside" of it. */
      r.x = floor (clip_box.bounds.origin.x);
      r.y = floor (clip_box.bounds.origin.y);
      r.width = ceil (clip_box.bounds.origin.x + clip_box.bounds.size.width) - r.x;
      r.height = ceil (clip_box.bounds.origin.y + clip_box.bounds.size.height) - r.y;
      remaining = cairo_region_create_rectangle (&r);

      /* First do the corners of box */
      for (i = 0; i < 4; i++)
	{
	  cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
	  draw_shadow_corner (cr, TRUE, &box, &clip_box, self->blur_radius, &self->color, i, &r);
	  cairo_restore (cr);

	  /* We drew the region, remove it from remaining */
	  cairo_region_subtract_rectangle (remaining, &r);
	}

      /* Then the sides */
      for (i = 0; i < 4; i++)
	{
	  cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
	  draw_shadow_side (cr, TRUE, &box, &clip_box, self->blur_radius, &self->color, i, &r);
	  cairo_restore (cr);

	  /* We drew the region, remove it from remaining */
	  cairo_region_subtract_rectangle (remaining, &r);
	}

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      draw_shadow (cr, TRUE, &box, &clip_box, self->blur_radius, &self->color, GSK_BLUR_NONE);
      cairo_restore (cr);

      cairo_region_destroy (remaining);
    }

  cairo_restore (cr);
}

static void
gsk_inset_shadow_node_diff (GskRenderNode  *node1,
                            GskRenderNode  *node2,
                            cairo_region_t *region)
{
  GskInsetShadowNode *self1 = (GskInsetShadowNode *) node1;
  GskInsetShadowNode *self2 = (GskInsetShadowNode *) node2;

  if (gsk_rounded_rect_equal (&self1->outline, &self2->outline) &&
      gdk_rgba_equal (&self1->color, &self2->color) &&
      self1->dx == self2->dx &&
      self1->dy == self2->dy &&
      self1->spread == self2->spread &&
      self1->blur_radius == self2->blur_radius)
    return;

  gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_INSET_SHADOW_NODE_CLASS = {
  GSK_INSET_SHADOW_NODE,
  sizeof (GskInsetShadowNode),
  "GskInsetShadowNode",
  gsk_inset_shadow_node_finalize,
  gsk_inset_shadow_node_draw,
  gsk_render_node_can_diff_true,
  gsk_inset_shadow_node_diff,
};

/**
 * gsk_inset_shadow_node_new:
 * @outline: outline of the region containing the shadow
 * @color: color of the shadow
 * @dx: horizontal offset of shadow
 * @dy: vertical offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a #GskRenderNode that will render an inset shadow
 * into the box given by @outline.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_inset_shadow_node_new (const GskRoundedRect *outline,
                           const GdkRGBA        *color,
                           float                 dx,
                           float                 dy,
                           float                 spread,
                           float                 blur_radius)
{
  GskInsetShadowNode *self;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (color != NULL, NULL);

  self = (GskInsetShadowNode *) gsk_render_node_new (&GSK_INSET_SHADOW_NODE_CLASS, 0);

  gsk_rounded_rect_init_copy (&self->outline, outline);
  self->color = *color;
  self->dx = dx;
  self->dy = dy;
  self->spread = spread;
  self->blur_radius = blur_radius;

  graphene_rect_init_from_rect (&self->render_node.bounds, &self->outline.bounds);

  return &self->render_node;
}

const GskRoundedRect *
gsk_inset_shadow_node_peek_outline (GskRenderNode *node)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_INSET_SHADOW_NODE), NULL);

  return &self->outline;
}

const GdkRGBA *
gsk_inset_shadow_node_peek_color (GskRenderNode *node)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_INSET_SHADOW_NODE), NULL);

  return &self->color;
}

float
gsk_inset_shadow_node_get_dx (GskRenderNode *node)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_INSET_SHADOW_NODE), 0.0f);

  return self->dx;
}

float
gsk_inset_shadow_node_get_dy (GskRenderNode *node)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_INSET_SHADOW_NODE), 0.0f);

  return self->dy;
}

float
gsk_inset_shadow_node_get_spread (GskRenderNode *node)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_INSET_SHADOW_NODE), 0.0f);

  return self->spread;
}

float
gsk_inset_shadow_node_get_blur_radius (GskRenderNode *node)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_INSET_SHADOW_NODE), 0.0f);

  return self->blur_radius;
}

/*** GSK_OUTSET_SHADOW_NODE ***/

typedef struct _GskOutsetShadowNode GskOutsetShadowNode;

struct _GskOutsetShadowNode
{
  GskRenderNode render_node;

  GskRoundedRect outline;
  GdkRGBA color;
  float dx;
  float dy;
  float spread;
  float blur_radius;
};

static void
gsk_outset_shadow_node_finalize (GskRenderNode *node)
{
}

static void
gsk_outset_shadow_get_extents (GskOutsetShadowNode *self,
                               float               *top,
                               float               *right,
                               float               *bottom,
                               float               *left)
{
  float clip_radius;

  clip_radius = gsk_cairo_blur_compute_pixels (self->blur_radius);
  *top = MAX (0, clip_radius + self->spread - self->dy);
  *right = MAX (0, ceil (clip_radius + self->spread + self->dx));
  *bottom = MAX (0, ceil (clip_radius + self->spread + self->dy));
  *left = MAX (0, ceil (clip_radius + self->spread - self->dx));
}

static void
gsk_outset_shadow_node_draw (GskRenderNode *node,
                             cairo_t       *cr)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;
  GskRoundedRect box, clip_box;
  int clip_radius;
  double x1c, y1c, x2c, y2c;
  float top, right, bottom, left;

  /* We don't need to draw invisible shadows */
  if (gdk_rgba_is_clear (&self->color))
    return;

  cairo_clip_extents (cr, &x1c, &y1c, &x2c, &y2c);
  if (gsk_rounded_rect_contains_rect (&self->outline, &GRAPHENE_RECT_INIT (x1c, y1c, x2c - x1c, y2c - y1c)))
    return;

  clip_radius = gsk_cairo_blur_compute_pixels (self->blur_radius);

  cairo_save (cr);

  gsk_rounded_rect_init_copy (&clip_box, &self->outline);
  gsk_outset_shadow_get_extents (self, &top, &right, &bottom, &left);
  gsk_rounded_rect_shrink (&clip_box, -top, -right, -bottom, -left);

  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
  gsk_rounded_rect_path (&self->outline, cr);
  cairo_rectangle (cr,
                   clip_box.bounds.origin.x, clip_box.bounds.origin.y,
                   clip_box.bounds.size.width, clip_box.bounds.size.height);

  cairo_clip (cr);

  gsk_rounded_rect_init_copy (&box, &self->outline);
  gsk_rounded_rect_offset (&box, self->dx, self->dy);
  gsk_rounded_rect_shrink (&box, -self->spread, -self->spread, -self->spread, -self->spread);

  if (!needs_blur (self->blur_radius))
    draw_shadow (cr, FALSE, &box, &clip_box, self->blur_radius, &self->color, GSK_BLUR_NONE);
  else
    {
      int i;
      cairo_region_t *remaining;
      cairo_rectangle_int_t r;

      /* For the blurred case we divide the rendering into 9 parts,
       * 4 of the corners, 4 for the horizonat/vertical lines and
       * one for the interior. We make the non-interior parts
       * large enought to fit the full radius of the blur, so that
       * the interior part can be drawn solidly.
       */

      /* In the outset case we want to paint the entire box, plus as far
       * as the radius reaches from it */
      r.x = floor (box.bounds.origin.x - clip_radius);
      r.y = floor (box.bounds.origin.y - clip_radius);
      r.width = ceil (box.bounds.origin.x + box.bounds.size.width + clip_radius) - r.x;
      r.height = ceil (box.bounds.origin.y + box.bounds.size.height + clip_radius) - r.y;

      remaining = cairo_region_create_rectangle (&r);

      /* First do the corners of box */
      for (i = 0; i < 4; i++)
	{
	  cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
	  draw_shadow_corner (cr, FALSE, &box, &clip_box, self->blur_radius, &self->color, i, &r);
	  cairo_restore (cr);

	  /* We drew the region, remove it from remaining */
	  cairo_region_subtract_rectangle (remaining, &r);
	}

      /* Then the sides */
      for (i = 0; i < 4; i++)
	{
	  cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
	  draw_shadow_side (cr, FALSE, &box, &clip_box, self->blur_radius, &self->color, i, &r);
	  cairo_restore (cr);

	  /* We drew the region, remove it from remaining */
	  cairo_region_subtract_rectangle (remaining, &r);
	}

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      draw_shadow (cr, FALSE, &box, &clip_box, self->blur_radius, &self->color, GSK_BLUR_NONE);
      cairo_restore (cr);

      cairo_region_destroy (remaining);
    }

  cairo_restore (cr);
}

static void
gsk_outset_shadow_node_diff (GskRenderNode  *node1,
                             GskRenderNode  *node2,
                             cairo_region_t *region)
{
  GskOutsetShadowNode *self1 = (GskOutsetShadowNode *) node1;
  GskOutsetShadowNode *self2 = (GskOutsetShadowNode *) node2;

  if (gsk_rounded_rect_equal (&self1->outline, &self2->outline) &&
      gdk_rgba_equal (&self1->color, &self2->color) &&
      self1->dx == self2->dx &&
      self1->dy == self2->dy &&
      self1->spread == self2->spread &&
      self1->blur_radius == self2->blur_radius)
    return;

  gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_OUTSET_SHADOW_NODE_CLASS = {
  GSK_OUTSET_SHADOW_NODE,
  sizeof (GskOutsetShadowNode),
  "GskOutsetShadowNode",
  gsk_outset_shadow_node_finalize,
  gsk_outset_shadow_node_draw,
  gsk_render_node_can_diff_true,
  gsk_outset_shadow_node_diff,
};

/**
 * gsk_outset_shadow_node_new:
 * @outline: outline of the region surrounded by shadow
 * @color: color of the shadow
 * @dx: horizontal offset of shadow
 * @dy: vertical offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a #GskRenderNode that will render an outset shadow
 * around the box given by @outline.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_outset_shadow_node_new (const GskRoundedRect *outline,
                            const GdkRGBA        *color,
                            float                 dx,
                            float                 dy,
                            float                 spread,
                            float                 blur_radius)
{
  GskOutsetShadowNode *self;
  float top, right, bottom, left;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (color != NULL, NULL);

  self = (GskOutsetShadowNode *) gsk_render_node_new (&GSK_OUTSET_SHADOW_NODE_CLASS, 0);

  gsk_rounded_rect_init_copy (&self->outline, outline);
  self->color = *color;
  self->dx = dx;
  self->dy = dy;
  self->spread = spread;
  self->blur_radius = blur_radius;

  gsk_outset_shadow_get_extents (self, &top, &right, &bottom, &left);

  graphene_rect_init_from_rect (&self->render_node.bounds, &self->outline.bounds);

  self->render_node.bounds.origin.x -= left;
  self->render_node.bounds.origin.y -= top;
  self->render_node.bounds.size.width += left + right;
  self->render_node.bounds.size.height += top + bottom;

  return &self->render_node;
}

const GskRoundedRect *
gsk_outset_shadow_node_peek_outline (GskRenderNode *node)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OUTSET_SHADOW_NODE), NULL);

  return &self->outline;
}

const GdkRGBA *
gsk_outset_shadow_node_peek_color (GskRenderNode *node)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OUTSET_SHADOW_NODE), NULL);

  return &self->color;
}

float
gsk_outset_shadow_node_get_dx (GskRenderNode *node)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OUTSET_SHADOW_NODE), 0.0f);

  return self->dx;
}

float
gsk_outset_shadow_node_get_dy (GskRenderNode *node)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OUTSET_SHADOW_NODE), 0.0f);

  return self->dy;
}

float
gsk_outset_shadow_node_get_spread (GskRenderNode *node)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OUTSET_SHADOW_NODE), 0.0f);

  return self->spread;
}

float
gsk_outset_shadow_node_get_blur_radius (GskRenderNode *node)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OUTSET_SHADOW_NODE), 0.0f);

  return self->blur_radius;
}

/*** GSK_CAIRO_NODE ***/

typedef struct _GskCairoNode GskCairoNode;

struct _GskCairoNode
{
  GskRenderNode render_node;

  cairo_surface_t *surface;
};

static void
gsk_cairo_node_finalize (GskRenderNode *node)
{
  GskCairoNode *self = (GskCairoNode *) node;

  if (self->surface)
    cairo_surface_destroy (self->surface);
}

static void
gsk_cairo_node_draw (GskRenderNode *node,
                     cairo_t       *cr)
{
  GskCairoNode *self = (GskCairoNode *) node;

  if (self->surface == NULL)
    return;

  cairo_set_source_surface (cr, self->surface, 0, 0);
  cairo_paint (cr);
}

static const GskRenderNodeClass GSK_CAIRO_NODE_CLASS = {
  GSK_CAIRO_NODE,
  sizeof (GskCairoNode),
  "GskCairoNode",
  gsk_cairo_node_finalize,
  gsk_cairo_node_draw,
  gsk_render_node_can_diff_true,
  gsk_render_node_diff_impossible,
};

cairo_surface_t *
gsk_cairo_node_peek_surface (GskRenderNode *node)
{
  GskCairoNode *self = (GskCairoNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CAIRO_NODE), NULL);

  return self->surface;
}

/**
 * gsk_cairo_node_new:
 * @bounds: the rectangle to render to
 *
 * Creates a #GskRenderNode that will render a cairo surface
 * into the area given by @bounds. You can draw to the cairo
 * surface using gsk_cairo_node_get_draw_context()
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_cairo_node_new (const graphene_rect_t *bounds)
{
  GskCairoNode *self;

  g_return_val_if_fail (bounds != NULL, NULL);

  self = (GskCairoNode *) gsk_render_node_new (&GSK_CAIRO_NODE_CLASS, 0);

  graphene_rect_init_from_rect (&self->render_node.bounds, bounds);

  return &self->render_node;
}

/**
 * gsk_cairo_node_get_draw_context:
 * @node: a cairo #GskRenderNode
 *
 * Creates a Cairo context for drawing using the surface associated
 * to the render node.
 * If no surface exists yet, a surface will be created optimized for
 * rendering to @renderer.
 *
 * Returns: (transfer full): a Cairo context used for drawing; use
 *   cairo_destroy() when done drawing
 */
cairo_t *
gsk_cairo_node_get_draw_context (GskRenderNode *node)
{
  GskCairoNode *self = (GskCairoNode *) node;
  int width, height;
  cairo_t *res;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CAIRO_NODE), NULL);

  width = ceilf (node->bounds.size.width);
  height = ceilf (node->bounds.size.height);

  if (width <= 0 || height <= 0)
    {
      cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0);
      res = cairo_create (surface);
      cairo_surface_destroy (surface);
    }
  else if (self->surface == NULL)
    {
      self->surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA,
                                                      &(cairo_rectangle_t) {
                                                          node->bounds.origin.x,
                                                          node->bounds.origin.y,
                                                          node->bounds.size.width,
                                                          node->bounds.size.height
                                                      });
      res = cairo_create (self->surface);
    }
  else
    {
      res = cairo_create (self->surface);
    }

  cairo_rectangle (res,
                   node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_clip (res);

  return res;
}

/**** GSK_CONTAINER_NODE ***/

typedef struct _GskContainerNode GskContainerNode;

struct _GskContainerNode
{
  GskRenderNode render_node;

  guint n_children;
  GskRenderNode *children[];
};

static void
gsk_container_node_finalize (GskRenderNode *node)
{
  GskContainerNode *container = (GskContainerNode *) node;
  guint i;

  for (i = 0; i < container->n_children; i++)
    gsk_render_node_unref (container->children[i]);
}

static void
gsk_container_node_draw (GskRenderNode *node,
                         cairo_t       *cr)
{
  GskContainerNode *container = (GskContainerNode *) node;
  guint i;

  for (i = 0; i < container->n_children; i++)
    {
      gsk_render_node_draw (container->children[i], cr);
    }
}

static gboolean
gsk_container_node_can_diff (GskRenderNode *node1,
                             GskRenderNode *node2)
{
  return TRUE;
}

static void
gsk_render_node_add_to_region (GskRenderNode  *node,
                               cairo_region_t *region)
{
  cairo_rectangle_int_t rect;

  rectangle_init_from_graphene (&rect, &node->bounds);
  cairo_region_union_rectangle (region, &rect);
}

static int
gsk_container_node_compare_func (gconstpointer elem1, gconstpointer elem2, gpointer data)
{
  return gsk_render_node_can_diff ((GskRenderNode *) elem1, (GskRenderNode *) elem2) ? 0 : 1;
}

static void
gsk_container_node_keep_func (gconstpointer elem1, gconstpointer elem2, gpointer data)
{
  gsk_render_node_diff ((GskRenderNode *) elem1, (GskRenderNode *) elem2, data);
}

static void
gsk_container_node_change_func (gconstpointer elem, gsize idx, gpointer data)
{
  gsk_render_node_add_to_region ((GskRenderNode *) elem, data);
}

static GskDiffSettings *
gsk_container_node_get_diff_settings (void)
{
  static GskDiffSettings *settings = NULL;

  if (G_LIKELY (settings))
    return settings;

  settings = gsk_diff_settings_new (gsk_container_node_compare_func,
                                    gsk_container_node_keep_func,
                                    gsk_container_node_change_func,
                                    gsk_container_node_change_func);
  gsk_diff_settings_set_allow_abort (settings, TRUE);

  return settings;
}

static void
gsk_container_node_diff (GskRenderNode  *node1,
                         GskRenderNode  *node2,
                         cairo_region_t *region)
{
  GskContainerNode *self1 = (GskContainerNode *) node1;
  GskContainerNode *self2 = (GskContainerNode *) node2;

  if (gsk_diff ((gconstpointer *) self1->children,
                self1->n_children,
                (gconstpointer *) self2->children,
                self2->n_children,
                gsk_container_node_get_diff_settings (),
                region) == GSK_DIFF_OK)
    return;

  gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_CONTAINER_NODE_CLASS = {
  GSK_CONTAINER_NODE,
  sizeof (GskContainerNode),
  "GskContainerNode",
  gsk_container_node_finalize,
  gsk_container_node_draw,
  gsk_container_node_can_diff,
  gsk_container_node_diff,
};

/**
 * gsk_container_node_new:
 * @children: (array length=n_children) (transfer none): The children of the node
 * @n_children: Number of children in the @children array
 *
 * Creates a new #GskRenderNode instance for holding the given @children.
 * The new node will acquire a reference to each of the children.
 *
 * Returns: (transfer full): the new #GskRenderNode
 */
GskRenderNode *
gsk_container_node_new (GskRenderNode **children,
                        guint           n_children)
{
  GskContainerNode *container;
  guint i;

  container = (GskContainerNode *) gsk_render_node_new (&GSK_CONTAINER_NODE_CLASS, sizeof (GskRenderNode *) * n_children);

  container->n_children = n_children;

  if (n_children == 0)
    {
      graphene_rect_init_from_rect (&container->render_node.bounds, graphene_rect_zero());
    }
  else
    {
      graphene_rect_t bounds;

      container->children[0] = gsk_render_node_ref (children[0]);
      graphene_rect_init_from_rect (&bounds, &container->children[0]->bounds);
      for (i = 1; i < n_children; i++)
        {
          container->children[i] = gsk_render_node_ref (children[i]);
          graphene_rect_union (&bounds, &children[i]->bounds, &bounds);
        }

      graphene_rect_init_from_rect (&container->render_node.bounds, &bounds);
    }

  return &container->render_node;
}

/**
 * gsk_container_node_get_n_children:
 * @node: a container #GskRenderNode
 *
 * Retrieves the number of direct children of @node.
 *
 * Returns: the number of children of the #GskRenderNode
 */
guint
gsk_container_node_get_n_children (GskRenderNode *node)
{
  GskContainerNode *container = (GskContainerNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE), 0);

  return container->n_children;
}

/**
 * gsk_container_node_get_child:
 * @node: a container #GskRenderNode
 * @idx: the position of the child to get
 *
 * Gets one of the children of @container.
 *
 * Returns: the @idx'th child of @container
 */
GskRenderNode *
gsk_container_node_get_child (GskRenderNode *node,
                              guint          idx)
{
  GskContainerNode *container = (GskContainerNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE), NULL);
  g_return_val_if_fail (idx < container->n_children, 0);

  return container->children[idx];
}

/*** GSK_TRANSFORM_NODE ***/

typedef struct _GskTransformNode GskTransformNode;

struct _GskTransformNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GskTransform *transform;
};

static void
gsk_transform_node_finalize (GskRenderNode *node)
{
  GskTransformNode *self = (GskTransformNode *) node;

  gsk_render_node_unref (self->child);
  gsk_transform_unref (self->transform);
}

static void
gsk_transform_node_draw (GskRenderNode *node,
                         cairo_t       *cr)
{
  GskTransformNode *self = (GskTransformNode *) node;
  float xx, yx, xy, yy, dx, dy;
  cairo_matrix_t ctm;

  if (gsk_transform_get_category (self->transform) < GSK_TRANSFORM_CATEGORY_2D)
    {
      cairo_set_source_rgb (cr, 255 / 255., 105 / 255., 180 / 255.);
      cairo_rectangle (cr, node->bounds.origin.x, node->bounds.origin.y, node->bounds.size.width, node->bounds.size.height);
      cairo_fill (cr);
      return;
    }

  gsk_transform_to_2d (self->transform, &xx, &yx, &xy, &yy, &dx, &dy);
  cairo_matrix_init (&ctm, xx, yx, xy, yy, dx, dy);
  GSK_NOTE (CAIRO, g_message ("CTM = { .xx = %g, .yx = %g, .xy = %g, .yy = %g, .x0 = %g, .y0 = %g }",
                            ctm.xx, ctm.yx,
                            ctm.xy, ctm.yy,
                            ctm.x0, ctm.y0));
  cairo_transform (cr, &ctm);
  
  gsk_render_node_draw (self->child, cr);
}

static gboolean
gsk_transform_node_can_diff (GskRenderNode *node1,
                             GskRenderNode *node2)
{
  GskTransformNode *self1 = (GskTransformNode *) node1;
  GskTransformNode *self2 = (GskTransformNode *) node2;

  if (!gsk_transform_equal (self1->transform, self2->transform))
    return FALSE;

  return gsk_render_node_can_diff (self1->child, self2->child);
}

static void
gsk_transform_node_diff (GskRenderNode  *node1,
                         GskRenderNode  *node2,
                         cairo_region_t *region)
{
  GskTransformNode *self1 = (GskTransformNode *) node1;
  GskTransformNode *self2 = (GskTransformNode *) node2;

  if (!gsk_transform_equal (self1->transform, self2->transform))
    {
      gsk_render_node_diff_impossible (node1, node2, region);
      return;
    }

  if (self1->child == self2->child)
    return;

  switch (gsk_transform_get_category (self1->transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      gsk_render_node_diff (self1->child, self2->child, region);
      break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        cairo_region_t *sub;
        float dx, dy;

        gsk_transform_to_translate (self1->transform, &dx, &dy);
        sub = cairo_region_create ();
        gsk_render_node_diff (self1->child, self2->child, sub);
        cairo_region_translate (sub, floor (dx), floor (dy));
        if (floor (dx) != dx)
          {
            cairo_region_t *tmp = cairo_region_copy (sub);
            cairo_region_translate (tmp, 1, 0);
            cairo_region_union (sub, tmp);
            cairo_region_destroy (sub);
          }
        if (floor (dy) != dy)
          {
            cairo_region_t *tmp = cairo_region_copy (sub);
            cairo_region_translate (tmp, 0, 1);
            cairo_region_union (sub, tmp);
            cairo_region_destroy (sub);
          }
        cairo_region_union (region, sub);
        cairo_region_destroy (sub);
      }
      break;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
    default:
      gsk_render_node_diff_impossible (node1, node2, region);
      break;
    }
}

static const GskRenderNodeClass GSK_TRANSFORM_NODE_CLASS = {
  GSK_TRANSFORM_NODE,
  sizeof (GskTransformNode),
  "GskTransformNode",
  gsk_transform_node_finalize,
  gsk_transform_node_draw,
  gsk_transform_node_can_diff,
  gsk_transform_node_diff,
};

/**
 * gsk_transform_node_new:
 * @child: The node to transform
 * @transform: (transfer none): The transform to apply
 *
 * Creates a #GskRenderNode that will transform the given @child
 * with the given @transform.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_transform_node_new (GskRenderNode *child,
                        GskTransform  *transform)
{
  GskTransformNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (transform != NULL, NULL);

  self = (GskTransformNode *) gsk_render_node_new (&GSK_TRANSFORM_NODE_CLASS, 0);

  self->child = gsk_render_node_ref (child);
  self->transform = gsk_transform_ref (transform);

  gsk_transform_transform_bounds (self->transform,
                                  &child->bounds,
                                  &self->render_node.bounds);

  return &self->render_node;
}

/**
 * gsk_transform_node_get_child:
 * @node: a transform @GskRenderNode
 *
 * Gets the child node that is getting transformed by the given @node.
 *
 * Returns: (transfer none): The child that is getting transformed
 **/
GskRenderNode *
gsk_transform_node_get_child (GskRenderNode *node)
{
  GskTransformNode *self = (GskTransformNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TRANSFORM_NODE), NULL);

  return self->child;
}

GskTransform *
gsk_transform_node_get_transform (GskRenderNode *node)
{
  GskTransformNode *self = (GskTransformNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TRANSFORM_NODE), NULL);

  return self->transform;
}

/*** GSK_DEBUG_NODE ***/

typedef struct _GskDebugNode GskDebugNode;

struct _GskDebugNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  char *message;
};

static void
gsk_debug_node_finalize (GskRenderNode *node)
{
  GskDebugNode *self = (GskDebugNode *) node;

  gsk_render_node_unref (self->child);
  g_free (self->message);
}

static void
gsk_debug_node_draw (GskRenderNode *node,
                      cairo_t       *cr)
{
  GskDebugNode *self = (GskDebugNode *) node;

  gsk_render_node_draw (self->child, cr);
}

static gboolean
gsk_debug_node_can_diff (GskRenderNode *node1,
                         GskRenderNode *node2)
{
  GskDebugNode *self1 = (GskDebugNode *) node1;
  GskDebugNode *self2 = (GskDebugNode *) node2;

  return gsk_render_node_can_diff (self1->child, self2->child);
}

static void
gsk_debug_node_diff (GskRenderNode  *node1,
                     GskRenderNode  *node2,
                     cairo_region_t *region)
{
  GskDebugNode *self1 = (GskDebugNode *) node1;
  GskDebugNode *self2 = (GskDebugNode *) node2;

  gsk_render_node_diff (self1->child, self2->child, region);
}

static const GskRenderNodeClass GSK_DEBUG_NODE_CLASS = {
  GSK_DEBUG_NODE,
  sizeof (GskDebugNode),
  "GskDebugNode",
  gsk_debug_node_finalize,
  gsk_debug_node_draw,
  gsk_debug_node_can_diff,
  gsk_debug_node_diff,
};

/**
 * gsk_debug_node_new:
 * @child: The child to add debug info for
 * @message: (transfer full): The debug message 
 *
 * Creates a #GskRenderNode that will add debug information about
 * the given @child.
 *
 * Adding this node has no visual effect.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_debug_node_new (GskRenderNode *child,
                    char          *message)
{
  GskDebugNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = (GskDebugNode *) gsk_render_node_new (&GSK_DEBUG_NODE_CLASS, 0);

  self->child = gsk_render_node_ref (child);
  self->message = message;

  graphene_rect_init_from_rect (&self->render_node.bounds, &child->bounds);

  return &self->render_node;
}

/**
 * gsk_debug_node_get_child:
 * @node: a debug @GskRenderNode
 *
 * Gets the child node that is getting debug by the given @node.
 *
 * Returns: (transfer none): The child that is getting debug
 **/
GskRenderNode *
gsk_debug_node_get_child (GskRenderNode *node)
{
  GskDebugNode *self = (GskDebugNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_DEBUG_NODE), NULL);

  return self->child;
}

/**
 * gsk_debug_node_get_message:
 * @node: a debug #GskRenderNode
 *
 * Gets the debug message that was set on this node
 *
 * Returns: (transfer none): The debug message
 **/
const char *
gsk_debug_node_get_message (GskRenderNode *node)
{
  GskDebugNode *self = (GskDebugNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_DEBUG_NODE), "You run broken code!");

  return self->message;
}

/*** GSK_OPACITY_NODE ***/

typedef struct _GskOpacityNode GskOpacityNode;

struct _GskOpacityNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  double opacity;
};

static void
gsk_opacity_node_finalize (GskRenderNode *node)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  gsk_render_node_unref (self->child);
}

static void
gsk_opacity_node_draw (GskRenderNode *node,
                       cairo_t       *cr)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  cairo_save (cr);

  /* clip so the push_group() creates a smaller surface */
  cairo_rectangle (cr, node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_clip (cr);

  cairo_push_group (cr);

  gsk_render_node_draw (self->child, cr);

  cairo_pop_group_to_source (cr);
  cairo_paint_with_alpha (cr, self->opacity);

  cairo_restore (cr);
}

static void
gsk_opacity_node_diff (GskRenderNode  *node1,
                       GskRenderNode  *node2,
                       cairo_region_t *region)
{
  GskOpacityNode *self1 = (GskOpacityNode *) node1;
  GskOpacityNode *self2 = (GskOpacityNode *) node2;

  if (self1->opacity == self2->opacity)
    gsk_render_node_diff (self1->child, self2->child, region);
  else
    gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_OPACITY_NODE_CLASS = {
  GSK_OPACITY_NODE,
  sizeof (GskOpacityNode),
  "GskOpacityNode",
  gsk_opacity_node_finalize,
  gsk_opacity_node_draw,
  gsk_render_node_can_diff_true,
  gsk_opacity_node_diff,
};

/**
 * gsk_opacity_node_new:
 * @child: The node to draw
 * @opacity: The opacity to apply
 *
 * Creates a #GskRenderNode that will drawn the @child with reduced
 * @opacity.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_opacity_node_new (GskRenderNode *child,
                      double         opacity)
{
  GskOpacityNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = (GskOpacityNode *) gsk_render_node_new (&GSK_OPACITY_NODE_CLASS, 0);

  self->child = gsk_render_node_ref (child);
  self->opacity = CLAMP (opacity, 0.0, 1.0);

  graphene_rect_init_from_rect (&self->render_node.bounds, &child->bounds);

  return &self->render_node;
}

/**
 * gsk_opacity_node_get_child:
 * @node: a opacity @GskRenderNode
 *
 * Gets the child node that is getting opacityed by the given @node.
 *
 * Returns: (transfer none): The child that is getting opacityed
 **/
GskRenderNode *
gsk_opacity_node_get_child (GskRenderNode *node)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OPACITY_NODE), NULL);

  return self->child;
}

double
gsk_opacity_node_get_opacity (GskRenderNode *node)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_OPACITY_NODE), 1.0);

  return self->opacity;
}

/*** GSK_COLOR_MATRIX_NODE ***/

typedef struct _GskColorMatrixNode GskColorMatrixNode;

struct _GskColorMatrixNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  graphene_matrix_t color_matrix;
  graphene_vec4_t color_offset;
};

static void
gsk_color_matrix_node_finalize (GskRenderNode *node)
{
  GskColorMatrixNode *self = (GskColorMatrixNode *) node;

  gsk_render_node_unref (self->child);
}

static void
gsk_color_matrix_node_draw (GskRenderNode *node,
                            cairo_t       *cr)
{
  GskColorMatrixNode *self = (GskColorMatrixNode *) node;
  cairo_pattern_t *pattern;
  cairo_surface_t *surface, *image_surface;
  graphene_vec4_t pixel;
  guint32* pixel_data;
  guchar *data;
  gsize x, y, width, height, stride;
  float alpha;

  cairo_save (cr);

  /* clip so the push_group() creates a smaller surface */
  cairo_rectangle (cr, node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_clip (cr);

  cairo_push_group (cr);

  gsk_render_node_draw (self->child, cr);

  pattern = cairo_pop_group (cr);
  cairo_pattern_get_surface (pattern, &surface);
  image_surface = cairo_surface_map_to_image (surface, NULL);

  data = cairo_image_surface_get_data (image_surface);
  width = cairo_image_surface_get_width (image_surface);
  height = cairo_image_surface_get_height (image_surface);
  stride = cairo_image_surface_get_stride (image_surface);

  for (y = 0; y < height; y++)
    {
      pixel_data = (guint32 *) data;
      for (x = 0; x < width; x++)
        {
          alpha = ((pixel_data[x] >> 24) & 0xFF) / 255.0;

          if (alpha == 0)
            {
              graphene_vec4_init (&pixel, 0.0, 0.0, 0.0, 0.0);
            }
          else
            {
              graphene_vec4_init (&pixel,
                                  ((pixel_data[x] >> 16) & 0xFF) / (255.0 * alpha),
                                  ((pixel_data[x] >>  8) & 0xFF) / (255.0 * alpha),
                                  ( pixel_data[x]        & 0xFF) / (255.0 * alpha),
                                  alpha);
              graphene_matrix_transform_vec4 (&self->color_matrix, &pixel, &pixel);
            }

          graphene_vec4_add (&pixel, &self->color_offset, &pixel);

          alpha = graphene_vec4_get_w (&pixel);
          if (alpha > 0.0)
            {
              alpha = MIN (alpha, 1.0);
              pixel_data[x] = (((guint32) roundf (alpha * 255)) << 24) |
                              (((guint32) roundf (CLAMP (graphene_vec4_get_x (&pixel), 0, 1) * alpha * 255)) << 16) |
                              (((guint32) roundf (CLAMP (graphene_vec4_get_y (&pixel), 0, 1) * alpha * 255)) <<  8) |
                               ((guint32) roundf (CLAMP (graphene_vec4_get_z (&pixel), 0, 1) * alpha * 255));
            }
          else
            {
              pixel_data[x] = 0;
            }
        }
      data += stride;
    }

  cairo_surface_mark_dirty (image_surface);
  cairo_surface_unmap_image (surface, image_surface);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_restore (cr);
  cairo_pattern_destroy (pattern);
}

static const GskRenderNodeClass GSK_COLOR_MATRIX_NODE_CLASS = {
  GSK_COLOR_MATRIX_NODE,
  sizeof (GskColorMatrixNode),
  "GskColorMatrixNode",
  gsk_color_matrix_node_finalize,
  gsk_color_matrix_node_draw,
  gsk_render_node_can_diff_true,
  gsk_render_node_diff_impossible,
};

/**
 * gsk_color_matrix_node_new:
 * @child: The node to draw
 * @color_matrix: The matrix to apply
 * @color_offset: Values to add to the color
 *
 * Creates a #GskRenderNode that will drawn the @child with reduced
 * @color_matrix.
 *
 * In particular, the node will transform the operation
 *   pixel = color_matrix * pixel + color_offset
 * for every pixel.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_color_matrix_node_new (GskRenderNode           *child,
                           const graphene_matrix_t *color_matrix,
                           const graphene_vec4_t   *color_offset)
{
  GskColorMatrixNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = (GskColorMatrixNode *) gsk_render_node_new (&GSK_COLOR_MATRIX_NODE_CLASS, 0);

  self->child = gsk_render_node_ref (child);
  graphene_matrix_init_from_matrix (&self->color_matrix, color_matrix);
  graphene_vec4_init_from_vec4 (&self->color_offset, color_offset);

  graphene_rect_init_from_rect (&self->render_node.bounds, &child->bounds);

  return &self->render_node;
}

/**
 * gsk_color_matrix_node_get_child:
 * @node: a color matrix @GskRenderNode
 *
 * Gets the child node that is getting its colors modified by the given @node.
 *
 * Returns: (transfer none): The child that is getting its colors modified
 **/
GskRenderNode *
gsk_color_matrix_node_get_child (GskRenderNode *node)
{
  GskColorMatrixNode *self = (GskColorMatrixNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_COLOR_MATRIX_NODE), NULL);

  return self->child;
}

const graphene_matrix_t *
gsk_color_matrix_node_peek_color_matrix (GskRenderNode *node)
{
  GskColorMatrixNode *self = (GskColorMatrixNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_COLOR_MATRIX_NODE), NULL);

  return &self->color_matrix;
}

const graphene_vec4_t *
gsk_color_matrix_node_peek_color_offset (GskRenderNode *node)
{
  GskColorMatrixNode *self = (GskColorMatrixNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_COLOR_MATRIX_NODE), NULL);

  return &self->color_offset;
}

/*** GSK_REPEAT_NODE ***/

typedef struct _GskRepeatNode GskRepeatNode;

struct _GskRepeatNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  graphene_rect_t child_bounds;
};

static void
gsk_repeat_node_finalize (GskRenderNode *node)
{
  GskRepeatNode *self = (GskRepeatNode *) node;

  gsk_render_node_unref (self->child);
}

static void
gsk_repeat_node_draw (GskRenderNode *node,
                      cairo_t       *cr)
{
  GskRepeatNode *self = (GskRepeatNode *) node;
  cairo_pattern_t *pattern;
  cairo_surface_t *surface;
  cairo_t *surface_cr;

  surface = cairo_surface_create_similar (cairo_get_target (cr),
                                          CAIRO_CONTENT_COLOR_ALPHA,
                                          ceilf (self->child_bounds.size.width),
                                          ceilf (self->child_bounds.size.height));
  surface_cr = cairo_create (surface);
  cairo_translate (surface_cr,
                   - self->child_bounds.origin.x,
                   - self->child_bounds.origin.y);
  gsk_render_node_draw (self->child, surface_cr);
  cairo_destroy (surface_cr);

  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  cairo_pattern_set_matrix (pattern,
                            &(cairo_matrix_t) {
                                .xx = 1.0,
                                .yy = 1.0,
                                .x0 = - self->child_bounds.origin.x,
                                .y0 = - self->child_bounds.origin.y
                            });

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);
}

static const GskRenderNodeClass GSK_REPEAT_NODE_CLASS = {
  GSK_REPEAT_NODE,
  sizeof (GskRepeatNode),
  "GskRepeatNode",
  gsk_repeat_node_finalize,
  gsk_repeat_node_draw,
  gsk_render_node_can_diff_true,
  gsk_render_node_diff_impossible,
};

/**
 * gsk_repeat_node_new:
 * @bounds: The bounds of the area to be painted
 * @child: The child to repeat
 * @child_bounds: (allow-none): The area of the child to repeat or %NULL to
 *     use the child's bounds
 *
 * Creates a #GskRenderNode that will repeat the drawing of @child across
 * the given @bounds.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_repeat_node_new (const graphene_rect_t *bounds,
                     GskRenderNode         *child,
                     const graphene_rect_t *child_bounds)
{
  GskRepeatNode *self;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = (GskRepeatNode *) gsk_render_node_new (&GSK_REPEAT_NODE_CLASS, 0);

  graphene_rect_init_from_rect (&self->render_node.bounds, bounds);
  self->child = gsk_render_node_ref (child);
  if (child_bounds)
    graphene_rect_init_from_rect (&self->child_bounds, child_bounds);
  else
    graphene_rect_init_from_rect (&self->child_bounds, &child->bounds);

  return &self->render_node;
}

GskRenderNode *
gsk_repeat_node_get_child (GskRenderNode *node)
{
  GskRepeatNode *self = (GskRepeatNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_REPEAT_NODE), NULL);

  return self->child;
}

const graphene_rect_t *
gsk_repeat_node_peek_child_bounds (GskRenderNode *node)
{
  GskRepeatNode *self = (GskRepeatNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_REPEAT_NODE), NULL);

  return &self->child_bounds;
}

/*** GSK_CLIP_NODE ***/

typedef struct _GskClipNode GskClipNode;

struct _GskClipNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  graphene_rect_t clip;
};

static void
gsk_clip_node_finalize (GskRenderNode *node)
{
  GskClipNode *self = (GskClipNode *) node;

  gsk_render_node_unref (self->child);
}

static void
gsk_clip_node_draw (GskRenderNode *node,
                    cairo_t       *cr)
{
  GskClipNode *self = (GskClipNode *) node;

  cairo_save (cr);

  cairo_rectangle (cr,
                   self->clip.origin.x, self->clip.origin.y,
                   self->clip.size.width, self->clip.size.height);
  cairo_clip (cr);

  gsk_render_node_draw (self->child, cr);

  cairo_restore (cr);
}

static void
gsk_clip_node_diff (GskRenderNode  *node1,
                    GskRenderNode  *node2,
                    cairo_region_t *region)
{
  GskClipNode *self1 = (GskClipNode *) node1;
  GskClipNode *self2 = (GskClipNode *) node2;

  if (graphene_rect_equal (&self1->clip, &self2->clip))
    {
      cairo_region_t *sub;
      cairo_rectangle_int_t clip_rect;

      sub = cairo_region_create();
      gsk_render_node_diff (self1->child, self2->child, sub);
      rectangle_init_from_graphene (&clip_rect, &self1->clip);
      cairo_region_intersect_rectangle (sub, &clip_rect);
      cairo_region_union (region, sub);
      cairo_region_destroy (sub);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, region);
    }
}

static const GskRenderNodeClass GSK_CLIP_NODE_CLASS = {
  GSK_CLIP_NODE,
  sizeof (GskClipNode),
  "GskClipNode",
  gsk_clip_node_finalize,
  gsk_clip_node_draw,
  gsk_render_node_can_diff_true,
  gsk_clip_node_diff,
};

/**
 * gsk_clip_node_new:
 * @child: The node to draw
 * @clip: The clip to apply
 *
 * Creates a #GskRenderNode that will clip the @child to the area
 * given by @clip.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_clip_node_new (GskRenderNode         *child,
                   const graphene_rect_t *clip)
{
  GskClipNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (clip != NULL, NULL);

  self = (GskClipNode *) gsk_render_node_new (&GSK_CLIP_NODE_CLASS, 0);

  self->child = gsk_render_node_ref (child);
  graphene_rect_normalize_r (clip, &self->clip);

  graphene_rect_intersection (&self->clip, &child->bounds, &self->render_node.bounds);

  return &self->render_node;
}

/**
 * gsk_clip_node_get_child:
 * @node: a clip @GskRenderNode
 *
 * Gets the child node that is getting clipped by the given @node.
 *
 * Returns: (transfer none): The child that is getting clipped
 **/
GskRenderNode *
gsk_clip_node_get_child (GskRenderNode *node)
{
  GskClipNode *self = (GskClipNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CLIP_NODE), NULL);

  return self->child;
}

const graphene_rect_t *
gsk_clip_node_peek_clip (GskRenderNode *node)
{
  GskClipNode *self = (GskClipNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CLIP_NODE), NULL);

  return &self->clip;
}

/*** GSK_ROUNDED_CLIP_NODE ***/

typedef struct _GskRoundedClipNode GskRoundedClipNode;

struct _GskRoundedClipNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GskRoundedRect clip;
};

static void
gsk_rounded_clip_node_finalize (GskRenderNode *node)
{
  GskRoundedClipNode *self = (GskRoundedClipNode *) node;

  gsk_render_node_unref (self->child);
}

static void
gsk_rounded_clip_node_draw (GskRenderNode *node,
                            cairo_t       *cr)
{
  GskRoundedClipNode *self = (GskRoundedClipNode *) node;

  cairo_save (cr);

  gsk_rounded_rect_path (&self->clip, cr);
  cairo_clip (cr);

  gsk_render_node_draw (self->child, cr);

  cairo_restore (cr);
}

static void
gsk_rounded_clip_node_diff (GskRenderNode  *node1,
                            GskRenderNode  *node2,
                            cairo_region_t *region)
{
  GskRoundedClipNode *self1 = (GskRoundedClipNode *) node1;
  GskRoundedClipNode *self2 = (GskRoundedClipNode *) node2;

  if (gsk_rounded_rect_equal (&self1->clip, &self2->clip))
    {
      cairo_region_t *sub;
      cairo_rectangle_int_t clip_rect;

      sub = cairo_region_create();
      gsk_render_node_diff (self1->child, self2->child, sub);
      rectangle_init_from_graphene (&clip_rect, &self1->clip.bounds);
      cairo_region_intersect_rectangle (sub, &clip_rect);
      cairo_region_union (region, sub);
      cairo_region_destroy (sub);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, region);
    }
}

static const GskRenderNodeClass GSK_ROUNDED_CLIP_NODE_CLASS = {
  GSK_ROUNDED_CLIP_NODE,
  sizeof (GskRoundedClipNode),
  "GskRoundedClipNode",
  gsk_rounded_clip_node_finalize,
  gsk_rounded_clip_node_draw,
  gsk_render_node_can_diff_true,
  gsk_rounded_clip_node_diff,
};

/**
 * gsk_rounded_clip_node_new:
 * @child: The node to draw
 * @clip: The clip to apply
 *
 * Creates a #GskRenderNode that will clip the @child to the area
 * given by @clip.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_rounded_clip_node_new (GskRenderNode         *child,
                           const GskRoundedRect  *clip)
{
  GskRoundedClipNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (clip != NULL, NULL);

  self = (GskRoundedClipNode *) gsk_render_node_new (&GSK_ROUNDED_CLIP_NODE_CLASS, 0);

  self->child = gsk_render_node_ref (child);
  gsk_rounded_rect_init_copy (&self->clip, clip);

  graphene_rect_intersection (&self->clip.bounds, &child->bounds, &self->render_node.bounds);

  return &self->render_node;
}

/**
 * gsk_rounded_clip_node_get_child:
 * @node: a clip @GskRenderNode
 *
 * Gets the child node that is getting clipped by the given @node.
 *
 * Returns: (transfer none): The child that is getting clipped
 **/
GskRenderNode *
gsk_rounded_clip_node_get_child (GskRenderNode *node)
{
  GskRoundedClipNode *self = (GskRoundedClipNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_ROUNDED_CLIP_NODE), NULL);

  return self->child;
}

const GskRoundedRect *
gsk_rounded_clip_node_peek_clip (GskRenderNode *node)
{
  GskRoundedClipNode *self = (GskRoundedClipNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_ROUNDED_CLIP_NODE), NULL);

  return &self->clip;
}

/*** GSK_SHADOW_NODE ***/

typedef struct _GskShadowNode GskShadowNode;

struct _GskShadowNode
{
  GskRenderNode render_node;

  GskRenderNode *child;

  gsize n_shadows;
  GskShadow shadows[];
};

static void
gsk_shadow_node_finalize (GskRenderNode *node)
{
  GskShadowNode *self = (GskShadowNode *) node;

  gsk_render_node_unref (self->child);
}

static void
gsk_shadow_node_draw (GskRenderNode *node,
                      cairo_t       *cr)
{
  GskShadowNode *self = (GskShadowNode *) node;
  cairo_pattern_t *pattern;
  gsize i;

  cairo_push_group (cr);
  gsk_render_node_draw (self->child, cr);
  pattern = cairo_pop_group (cr);

  for (i = 0; i < self->n_shadows; i++)
    {
      GskShadow *shadow = &self->shadows[i];

      /* We don't need to draw invisible shadows */
      if (gdk_rgba_is_clear (&shadow->color))
        continue;

      cairo_save (cr);
      gdk_cairo_set_source_rgba (cr, &shadow->color);
      cr = gsk_cairo_blur_start_drawing (cr, shadow->radius, GSK_BLUR_X | GSK_BLUR_Y);

      cairo_translate (cr, shadow->dx, shadow->dy);
      cairo_mask (cr, pattern);

      cr = gsk_cairo_blur_finish_drawing (cr, shadow->radius, &shadow->color, GSK_BLUR_X | GSK_BLUR_Y);
      cairo_restore (cr);
    }

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_pattern_destroy (pattern);
}

static void
gsk_shadow_node_diff (GskRenderNode  *node1,
                      GskRenderNode  *node2,
                      cairo_region_t *region)
{
  GskShadowNode *self1 = (GskShadowNode *) node1;
  GskShadowNode *self2 = (GskShadowNode *) node2;
  int top = 0, right = 0, bottom = 0, left = 0;
  cairo_region_t *sub;
  cairo_rectangle_int_t rect;
  gsize i, n;

  if (self1->n_shadows != self2->n_shadows)
    {
      gsk_render_node_diff_impossible (node1, node2, region);
      return;
    }

  for (i = 0; i < self1->n_shadows; i++)
    {
      GskShadow *shadow1 = &self1->shadows[i];
      GskShadow *shadow2 = &self2->shadows[i];
      float clip_radius;

      if (!gdk_rgba_equal (&shadow1->color, &shadow2->color) ||
          shadow1->dx != shadow2->dx ||
          shadow1->dy != shadow2->dy ||
          shadow1->radius != shadow2->radius)
        {
          gsk_render_node_diff_impossible (node1, node2, region);
          return;
        }

      clip_radius = gsk_cairo_blur_compute_pixels (shadow1->radius);
      top = MAX (top, ceil (clip_radius - shadow1->dy));
      right = MAX (right, ceil (clip_radius + shadow1->dx));
      bottom = MAX (bottom, ceil (clip_radius + shadow1->dy));
      left = MAX (left, ceil (clip_radius - shadow1->dx));
    }

  sub = cairo_region_create ();
  gsk_render_node_diff (self1->child, self2->child, sub);

  n = cairo_region_num_rectangles (sub);
  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (sub, i, &rect);
      rect.x -= left;
      rect.y -= top;
      rect.width += left + right;
      rect.height += top + bottom;
      cairo_region_union_rectangle (region, &rect);
    }
  cairo_region_destroy (sub);
}

static void
gsk_shadow_node_get_bounds (GskShadowNode *self,
                            graphene_rect_t *bounds)
{
  float top = 0, right = 0, bottom = 0, left = 0;
  gsize i;

  graphene_rect_init_from_rect (bounds, &self->child->bounds);

  for (i = 0; i < self->n_shadows; i++)
    {
      float clip_radius = gsk_cairo_blur_compute_pixels (self->shadows[i].radius);
      top = MAX (top, clip_radius - self->shadows[i].dy);
      right = MAX (right, clip_radius + self->shadows[i].dx);
      bottom = MAX (bottom, clip_radius + self->shadows[i].dy);
      left = MAX (left, clip_radius - self->shadows[i].dx);
    }

  bounds->origin.x -= left;
  bounds->origin.y -= top;
  bounds->size.width += left + right;
  bounds->size.height += top + bottom;
}

static const GskRenderNodeClass GSK_SHADOW_NODE_CLASS = {
  GSK_SHADOW_NODE,
  sizeof (GskShadowNode),
  "GskShadowNode",
  gsk_shadow_node_finalize,
  gsk_shadow_node_draw,
  gsk_render_node_can_diff_true,
  gsk_shadow_node_diff,
};

/**
 * gsk_shadow_node_new:
 * @child: The node to draw
 * @shadows: (array length=n_shadows): The shadows to apply
 * @n_shadows: number of entries in the @shadows array
 *
 * Creates a #GskRenderNode that will draw a @child with the given
 * @shadows below it.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_shadow_node_new (GskRenderNode         *child,
                     const GskShadow       *shadows,
                     gsize                  n_shadows)
{
  GskShadowNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (shadows != NULL, NULL);
  g_return_val_if_fail (n_shadows > 0, NULL);

  self = (GskShadowNode *) gsk_render_node_new (&GSK_SHADOW_NODE_CLASS, n_shadows * sizeof (GskShadow));

  self->child = gsk_render_node_ref (child);
  memcpy (&self->shadows, shadows, n_shadows * sizeof (GskShadow));
  self->n_shadows = n_shadows;

  gsk_shadow_node_get_bounds (self, &self->render_node.bounds);

  return &self->render_node;
}

GskRenderNode *
gsk_shadow_node_get_child (GskRenderNode *node)
{
  GskShadowNode *self = (GskShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_SHADOW_NODE), NULL);

  return self->child;
}

const GskShadow *
gsk_shadow_node_peek_shadow (GskRenderNode *node,
                             gsize          i)
{
  GskShadowNode *self = (GskShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_SHADOW_NODE), NULL);
  g_return_val_if_fail (i < self->n_shadows, NULL);

  return &self->shadows[i];
}

gsize
gsk_shadow_node_get_n_shadows (GskRenderNode *node)
{
  GskShadowNode *self = (GskShadowNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_SHADOW_NODE), 0);

  return self->n_shadows;
}

/*** GSK_BLEND_NODE ***/

typedef struct _GskBlendNode GskBlendNode;

struct _GskBlendNode
{
  GskRenderNode render_node;

  GskRenderNode *bottom;
  GskRenderNode *top;
  GskBlendMode blend_mode;
};

static cairo_operator_t
gsk_blend_mode_to_cairo_operator (GskBlendMode blend_mode)
{
  switch (blend_mode)
    {
    default:
      g_assert_not_reached ();
    case GSK_BLEND_MODE_DEFAULT:
      return CAIRO_OPERATOR_OVER;
    case GSK_BLEND_MODE_MULTIPLY:
      return CAIRO_OPERATOR_MULTIPLY;
    case GSK_BLEND_MODE_SCREEN:
      return CAIRO_OPERATOR_SCREEN;
    case GSK_BLEND_MODE_OVERLAY:
      return CAIRO_OPERATOR_OVERLAY;
    case GSK_BLEND_MODE_DARKEN:
      return CAIRO_OPERATOR_DARKEN;
    case GSK_BLEND_MODE_LIGHTEN:
      return CAIRO_OPERATOR_LIGHTEN;
    case GSK_BLEND_MODE_COLOR_DODGE:
      return CAIRO_OPERATOR_COLOR_DODGE;
    case GSK_BLEND_MODE_COLOR_BURN:
      return CAIRO_OPERATOR_COLOR_BURN;
    case GSK_BLEND_MODE_HARD_LIGHT:
      return CAIRO_OPERATOR_HARD_LIGHT;
    case GSK_BLEND_MODE_SOFT_LIGHT:
      return CAIRO_OPERATOR_SOFT_LIGHT;
    case GSK_BLEND_MODE_DIFFERENCE:
      return CAIRO_OPERATOR_DIFFERENCE;
    case GSK_BLEND_MODE_EXCLUSION:
      return CAIRO_OPERATOR_EXCLUSION;
    case GSK_BLEND_MODE_COLOR:
      return CAIRO_OPERATOR_HSL_COLOR;
    case GSK_BLEND_MODE_HUE:
      return CAIRO_OPERATOR_HSL_HUE;
    case GSK_BLEND_MODE_SATURATION:
      return CAIRO_OPERATOR_HSL_SATURATION;
    case GSK_BLEND_MODE_LUMINOSITY:
      return CAIRO_OPERATOR_HSL_LUMINOSITY;
    }
}

static void
gsk_blend_node_finalize (GskRenderNode *node)
{
  GskBlendNode *self = (GskBlendNode *) node;

  gsk_render_node_unref (self->bottom);
  gsk_render_node_unref (self->top);
}

static void
gsk_blend_node_draw (GskRenderNode *node,
                     cairo_t       *cr)
{
  GskBlendNode *self = (GskBlendNode *) node;

  cairo_push_group (cr);
  gsk_render_node_draw (self->bottom, cr);

  cairo_push_group (cr);
  gsk_render_node_draw (self->top, cr);

  cairo_pop_group_to_source (cr);
  cairo_set_operator (cr, gsk_blend_mode_to_cairo_operator (self->blend_mode));
  cairo_paint (cr);

  cairo_pop_group_to_source (cr); /* resets operator */
  cairo_paint (cr);
}

static void
gsk_blend_node_diff (GskRenderNode  *node1,
                     GskRenderNode  *node2,
                     cairo_region_t *region)
{
  GskBlendNode *self1 = (GskBlendNode *) node1;
  GskBlendNode *self2 = (GskBlendNode *) node2;

  if (self1->blend_mode == self2->blend_mode)
    {
      gsk_render_node_diff (self1->top, self2->top, region);
      gsk_render_node_diff (self1->bottom, self2->bottom, region);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, region);
    }
}

static const GskRenderNodeClass GSK_BLEND_NODE_CLASS = {
  GSK_BLEND_NODE,
  sizeof (GskBlendNode),
  "GskBlendNode",
  gsk_blend_node_finalize,
  gsk_blend_node_draw,
  gsk_render_node_can_diff_true,
  gsk_blend_node_diff,
};

/**
 * gsk_blend_node_new:
 * @bottom: The bottom node to be drawn
 * @top: The node to be blended onto the @bottom node
 * @blend_mode: The blend mode to use
 *
 * Creates a #GskRenderNode that will use @blend_mode to blend the @top
 * node onto the @bottom node.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_blend_node_new (GskRenderNode *bottom,
                    GskRenderNode *top,
                    GskBlendMode   blend_mode)
{
  GskBlendNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (bottom), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (top), NULL);

  self = (GskBlendNode *) gsk_render_node_new (&GSK_BLEND_NODE_CLASS, 0);

  self->bottom = gsk_render_node_ref (bottom);
  self->top = gsk_render_node_ref (top);
  self->blend_mode = blend_mode;

  graphene_rect_union (&bottom->bounds, &top->bounds, &self->render_node.bounds);

  return &self->render_node;
}

GskRenderNode *
gsk_blend_node_get_bottom_child (GskRenderNode *node)
{
  GskBlendNode *self = (GskBlendNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_BLEND_NODE), NULL);

  return self->bottom;
}

GskRenderNode *
gsk_blend_node_get_top_child (GskRenderNode *node)
{
  GskBlendNode *self = (GskBlendNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_BLEND_NODE), NULL);

  return self->top;
}

GskBlendMode
gsk_blend_node_get_blend_mode (GskRenderNode *node)
{
  GskBlendNode *self = (GskBlendNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_BLEND_NODE), GSK_BLEND_MODE_DEFAULT);

  return self->blend_mode;
}

/*** GSK_CROSS_FADE_NODE ***/

typedef struct _GskCrossFadeNode GskCrossFadeNode;

struct _GskCrossFadeNode
{
  GskRenderNode render_node;

  GskRenderNode *start;
  GskRenderNode *end;
  double         progress;
};

static void
gsk_cross_fade_node_finalize (GskRenderNode *node)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;

  gsk_render_node_unref (self->start);
  gsk_render_node_unref (self->end);
}

static void
gsk_cross_fade_node_draw (GskRenderNode *node,
                          cairo_t       *cr)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;

  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
  gsk_render_node_draw (self->start, cr);

  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
  gsk_render_node_draw (self->end, cr);

  cairo_pop_group_to_source (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint_with_alpha (cr, self->progress);

  cairo_pop_group_to_source (cr); /* resets operator */
  cairo_paint (cr);
}

static void
gsk_cross_fade_node_diff (GskRenderNode  *node1,
                          GskRenderNode  *node2,
                          cairo_region_t *region)
{
  GskCrossFadeNode *self1 = (GskCrossFadeNode *) node1;
  GskCrossFadeNode *self2 = (GskCrossFadeNode *) node2;

  if (self1->progress == self2->progress)
    {
      gsk_render_node_diff (self1->start, self2->start, region);
      gsk_render_node_diff (self1->end, self2->end, region);
      return;
    }

  gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_CROSS_FADE_NODE_CLASS = {
  GSK_CROSS_FADE_NODE,
  sizeof (GskCrossFadeNode),
  "GskCrossFadeNode",
  gsk_cross_fade_node_finalize,
  gsk_cross_fade_node_draw,
  gsk_render_node_can_diff_true,
  gsk_cross_fade_node_diff,
};

/**
 * gsk_cross_fade_node_new:
 * @start: The start node to be drawn
 * @end: The node to be cross_fadeed onto the @start node
 * @progress: How far the fade has progressed from start to end. The value will
 *     be clamped to the range [0 ... 1]
 *
 * Creates a #GskRenderNode that will do a cross-fade between @start and @end.
 *
 * Returns: A new #GskRenderNode
 */
GskRenderNode *
gsk_cross_fade_node_new (GskRenderNode *start,
                         GskRenderNode *end,
                         double         progress)
{
  GskCrossFadeNode *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (start), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (end), NULL);

  self = (GskCrossFadeNode *) gsk_render_node_new (&GSK_CROSS_FADE_NODE_CLASS, 0);

  self->start = gsk_render_node_ref (start);
  self->end = gsk_render_node_ref (end);
  self->progress = CLAMP (progress, 0.0, 1.0);

  graphene_rect_union (&start->bounds, &end->bounds, &self->render_node.bounds);

  return &self->render_node;
}

GskRenderNode *
gsk_cross_fade_node_get_start_child (GskRenderNode *node)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CROSS_FADE_NODE), NULL);

  return self->start;
}

GskRenderNode *
gsk_cross_fade_node_get_end_child (GskRenderNode *node)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CROSS_FADE_NODE), NULL);

  return self->end;
}

double
gsk_cross_fade_node_get_progress (GskRenderNode *node)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CROSS_FADE_NODE), 0.0);

  return self->progress;
}

/*** GSK_TEXT_NODE ***/

typedef struct _GskTextNode GskTextNode;

struct _GskTextNode
{
  GskRenderNode render_node;

  PangoFont *font;

  GdkRGBA color;
  graphene_point_t offset;

  guint num_glyphs;
  PangoGlyphInfo glyphs[];
};

static void
gsk_text_node_finalize (GskRenderNode *node)
{
  GskTextNode *self = (GskTextNode *) node;

  g_object_unref (self->font);
}

#ifndef STACK_BUFFER_SIZE
#define STACK_BUFFER_SIZE (512 * sizeof (int))
#endif

#define STACK_ARRAY_LENGTH(T) (STACK_BUFFER_SIZE / sizeof(T))

static void
gsk_text_node_draw (GskRenderNode *node,
                    cairo_t       *cr)
{
  GskTextNode *self = (GskTextNode *) node;
  PangoGlyphString glyphs;

  glyphs.num_glyphs = self->num_glyphs;
  glyphs.glyphs = self->glyphs;
  glyphs.log_clusters = NULL;

  cairo_save (cr);

  gdk_cairo_set_source_rgba (cr, &self->color);
  cairo_translate (cr, self->offset.x, self->offset.y);
  pango_cairo_show_glyph_string (cr, self->font, &glyphs);

  cairo_restore (cr);
}

static void
gsk_text_node_diff (GskRenderNode  *node1,
                    GskRenderNode  *node2,
                    cairo_region_t *region)
{
  GskTextNode *self1 = (GskTextNode *) node1;
  GskTextNode *self2 = (GskTextNode *) node2;

  if (self1->font == self2->font &&
      gdk_rgba_equal (&self1->color, &self2->color) &&
      graphene_point_equal (&self1->offset, &self2->offset) &&
      self1->num_glyphs == self2->num_glyphs)
    {
      guint i;

      for (i = 0; i < self1->num_glyphs; i++)
        {
          PangoGlyphInfo *info1 = &self1->glyphs[i];
          PangoGlyphInfo *info2 = &self2->glyphs[i];

          if (info1->glyph == info2->glyph &&
              info1->geometry.width == info2->geometry.width &&
              info1->geometry.x_offset == info2->geometry.x_offset &&
              info1->geometry.y_offset == info2->geometry.y_offset &&
              info1->attr.is_cluster_start == info2->attr.is_cluster_start)
            continue;

          gsk_render_node_diff_impossible (node1, node2, region);
          return;
        }

      return;
    }

  gsk_render_node_diff_impossible (node1, node2, region);
}

static const GskRenderNodeClass GSK_TEXT_NODE_CLASS = {
  GSK_TEXT_NODE,
  sizeof (GskTextNode),
  "GskTextNode",
  gsk_text_node_finalize,
  gsk_text_node_draw,
  gsk_render_node_can_diff_true,
  gsk_text_node_diff,
};

/**
 * gsk_text_node_new:
 * @font: the #PangoFont containing the glyphs
 * @glyphs: the #PangoGlyphString to render
 * @color: the foreground color to render with
 * @offset: offset of the baseline
 *
 * Creates a render node that renders the given glyphs,
 * Note that @color may not be used if the font contains
 * color glyphs.
 *
 * Returns: (nullable): a new text node, or %NULL
 */
GskRenderNode *
gsk_text_node_new (PangoFont              *font,
                   PangoGlyphString       *glyphs,
                   const GdkRGBA          *color,
                   const graphene_point_t *offset)
{
  GskTextNode *self;
  PangoRectangle ink_rect;

  pango_glyph_string_extents (glyphs, font, &ink_rect, NULL);
  pango_extents_to_pixels (&ink_rect, NULL);

  /* Don't create nodes with empty bounds */
  if (ink_rect.width == 0 || ink_rect.height == 0)
    return NULL;

  self = (GskTextNode *) gsk_render_node_new (&GSK_TEXT_NODE_CLASS, sizeof (PangoGlyphInfo) * glyphs->num_glyphs);

  self->font = g_object_ref (font);
  self->color = *color;
  self->offset = *offset;
  self->num_glyphs = glyphs->num_glyphs;
  memcpy (self->glyphs, glyphs->glyphs, sizeof (PangoGlyphInfo) * glyphs->num_glyphs);

  graphene_rect_init (&self->render_node.bounds,
                      offset->x + ink_rect.x - 1,
                      offset->y + ink_rect.y - 1,
                      ink_rect.width + 2,
                      ink_rect.height + 2);

  return &self->render_node;
}

const GdkRGBA *
gsk_text_node_peek_color (GskRenderNode *node)
{
  GskTextNode *self = (GskTextNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TEXT_NODE), NULL);

  return &self->color;
}

/**
 * gsk_text_node_peek_font:
 * @node: The #GskRenderNode
 *
 * Returns the font used by the text node.
 *
 * Returns: (transfer none): The used #PangoFont.
 */
PangoFont *
gsk_text_node_peek_font (GskRenderNode *node)
{
  GskTextNode *self = (GskTextNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TEXT_NODE), NULL);

  return self->font;
}

guint
gsk_text_node_get_num_glyphs (GskRenderNode *node)
{
  GskTextNode *self = (GskTextNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TEXT_NODE), 0);

  return self->num_glyphs;
}

const PangoGlyphInfo *
gsk_text_node_peek_glyphs (GskRenderNode *node)
{
  GskTextNode *self = (GskTextNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TEXT_NODE), NULL);

  return self->glyphs;
}

const graphene_point_t *
gsk_text_node_get_offset (GskRenderNode *node)
{
  GskTextNode *self = (GskTextNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_TEXT_NODE), NULL);

  return &self->offset;
}

/*** GSK_BLUR_NODE ***/

typedef struct _GskBlurNode GskBlurNode;

struct _GskBlurNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  double radius;
};

static void
gsk_blur_node_finalize (GskRenderNode *node)
{
  GskBlurNode *self = (GskBlurNode *) node;

  gsk_render_node_unref (self->child);
}

static void
blur_once (cairo_surface_t *src,
           cairo_surface_t *dest,
           int radius,
           guchar *div_kernel_size)
{
  int width, height, src_rowstride, dest_rowstride, n_channels;
  guchar *p_src, *p_dest, *c1, *c2;
  gint x, y, i, i1, i2, width_minus_1, height_minus_1, radius_plus_1;
  gint r, g, b, a;
  guchar *p_dest_row, *p_dest_col;

  width = cairo_image_surface_get_width (src);
  height = cairo_image_surface_get_height (src);
  n_channels = 4;
  radius_plus_1 = radius + 1;

  /* horizontal blur */
  p_src = cairo_image_surface_get_data (src);
  p_dest = cairo_image_surface_get_data (dest);
  src_rowstride = cairo_image_surface_get_stride (src);
  dest_rowstride = cairo_image_surface_get_stride (dest);

  width_minus_1 = width - 1;
  for (y = 0; y < height; y++)
    {
      /* calc the initial sums of the kernel */
      r = g = b = a = 0;
      for (i = -radius; i <= radius; i++)
        {
          c1 = p_src + (CLAMP (i, 0, width_minus_1) * n_channels);
          r += c1[0];
          g += c1[1];
          b += c1[2];
          a += c1[3];
        }
      p_dest_row = p_dest;
      for (x = 0; x < width; x++)
        {
          /* set as the mean of the kernel */
          p_dest_row[0] = div_kernel_size[r];
          p_dest_row[1] = div_kernel_size[g];
          p_dest_row[2] = div_kernel_size[b];
          p_dest_row[3] = div_kernel_size[a];
          p_dest_row += n_channels;

          /* the pixel to add to the kernel */
          i1 = x + radius_plus_1;
          if (i1 > width_minus_1)
            i1 = width_minus_1;
          c1 = p_src + (i1 * n_channels);

          /* the pixel to remove from the kernel */
          i2 = x - radius;
          if (i2 < 0)
            i2 = 0;
          c2 = p_src + (i2 * n_channels);

          /* calc the new sums of the kernel */
          r += c1[0] - c2[0];
          g += c1[1] - c2[1];
          b += c1[2] - c2[2];
          a += c1[3] - c2[3];
        }

      p_src += src_rowstride;
      p_dest += dest_rowstride;
    }

  /* vertical blur */
  p_src = cairo_image_surface_get_data (dest);
  p_dest = cairo_image_surface_get_data (src);
  src_rowstride = cairo_image_surface_get_stride (dest);
  dest_rowstride = cairo_image_surface_get_stride (src);

  height_minus_1 = height - 1;
  for (x = 0; x < width; x++)
    {
      /* calc the initial sums of the kernel */
      r = g = b = a = 0;
      for (i = -radius; i <= radius; i++)
        {
          c1 = p_src + (CLAMP (i, 0, height_minus_1) * src_rowstride);
          r += c1[0];
          g += c1[1];
          b += c1[2];
          a += c1[3];
        }

      p_dest_col = p_dest;
      for (y = 0; y < height; y++)
        {
          /* set as the mean of the kernel */

          p_dest_col[0] = div_kernel_size[r];
          p_dest_col[1] = div_kernel_size[g];
          p_dest_col[2] = div_kernel_size[b];
          p_dest_col[3] = div_kernel_size[a];
          p_dest_col += dest_rowstride;

          /* the pixel to add to the kernel */
          i1 = y + radius_plus_1;
          if (i1 > height_minus_1)
            i1 = height_minus_1;
          c1 = p_src + (i1 * src_rowstride);

          /* the pixel to remove from the kernel */
          i2 = y - radius;
          if (i2 < 0)
            i2 = 0;
          c2 = p_src + (i2 * src_rowstride);
          /* calc the new sums of the kernel */
          r += c1[0] - c2[0];
          g += c1[1] - c2[1];
          b += c1[2] - c2[2];
          a += c1[3] - c2[3];
        }

      p_src += n_channels;
      p_dest += n_channels;
    }
}

static void
blur_image_surface (cairo_surface_t *surface, int radius, int iterations)
{
  int kernel_size;
  int i;
  guchar *div_kernel_size;
  cairo_surface_t *tmp;
  int width, height;

  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);
  tmp = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  kernel_size = 2 * radius + 1;
  div_kernel_size = g_new (guchar, 256 * kernel_size);
  for (i = 0; i < 256 * kernel_size; i++)
    div_kernel_size[i] = (guchar) (i / kernel_size);

  while (iterations-- > 0)
    blur_once (surface, tmp, radius, div_kernel_size);

  g_free (div_kernel_size);
  cairo_surface_destroy (tmp);
}

static void
gsk_blur_node_draw (GskRenderNode *node,
                    cairo_t       *cr)
{
  GskBlurNode *self = (GskBlurNode *) node;
  cairo_pattern_t *pattern;
  cairo_surface_t *surface;
  cairo_surface_t *image_surface;

  cairo_save (cr);

  /* clip so the push_group() creates a smaller surface */
  cairo_rectangle (cr, node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_clip (cr);

  cairo_push_group (cr);

  gsk_render_node_draw (self->child, cr);

  pattern = cairo_pop_group (cr);
  cairo_pattern_get_surface (pattern, &surface);
  image_surface = cairo_surface_map_to_image (surface, NULL);
  blur_image_surface (image_surface, (int)self->radius, 3);
  cairo_surface_mark_dirty (surface);
  cairo_surface_unmap_image (surface, image_surface);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_restore (cr);
  cairo_pattern_destroy (pattern);
}

static void
gsk_blur_node_diff (GskRenderNode  *node1,
                    GskRenderNode  *node2,
                    cairo_region_t *region)
{
  GskBlurNode *self1 = (GskBlurNode *) node1;
  GskBlurNode *self2 = (GskBlurNode *) node2;

  if (self1->radius == self2->radius)
    {
      cairo_rectangle_int_t rect;
      cairo_region_t *sub;
      int i, n, clip_radius;

      clip_radius = ceil (gsk_cairo_blur_compute_pixels (self1->radius));
      sub = cairo_region_create ();
      gsk_render_node_diff (self1->child, self2->child, sub);

      n = cairo_region_num_rectangles (sub);
      for (i = 0; i < n; i++)
        {
          cairo_region_get_rectangle (sub, i, &rect);
          rect.x -= clip_radius;
          rect.y -= clip_radius;
          rect.width += 2 * clip_radius;
          rect.height += 2 * clip_radius;
          cairo_region_union_rectangle (region, &rect);
        }
      cairo_region_destroy (sub);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, region);
    }
}

static const GskRenderNodeClass GSK_BLUR_NODE_CLASS = {
  GSK_BLUR_NODE,
  sizeof (GskBlurNode),
  "GskBlurNode",
  gsk_blur_node_finalize,
  gsk_blur_node_draw,
  gsk_render_node_can_diff_true,
  gsk_blur_node_diff,
};

/**
 * gsk_blur_node_new:
 * @child: the child node to blur
 * @radius: the blur radius
 *
 * Creates a render node that blurs the child.
 */
GskRenderNode *
gsk_blur_node_new (GskRenderNode *child,
                   double         radius)
{
  GskBlurNode *self;
  float clip_radius = gsk_cairo_blur_compute_pixels (radius);

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = (GskBlurNode *) gsk_render_node_new (&GSK_BLUR_NODE_CLASS, 0);

  self->child = gsk_render_node_ref (child);
  self->radius = radius;

  graphene_rect_init_from_rect (&self->render_node.bounds, &child->bounds);
  graphene_rect_inset (&self->render_node.bounds,
                       - clip_radius, - clip_radius);

  return &self->render_node;
}

GskRenderNode *
gsk_blur_node_get_child (GskRenderNode *node)
{
  GskBlurNode *self = (GskBlurNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_BLUR_NODE), NULL);

  return self->child;
}

double
gsk_blur_node_get_radius (GskRenderNode *node)
{
  GskBlurNode *self = (GskBlurNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_BLUR_NODE), 0.0);

  return self->radius;
}

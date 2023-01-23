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
#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkprivate.h"

#include <hb-ot.h>
#include <hb-cairo.h>

/* maximal number of rectangles we keep in a diff region before we throw
 * the towel and just use the bounding box of the parent node.
 * Meant to avoid performance corner cases.
 */
#define MAX_RECTS_IN_DIFF 30

static inline void
gsk_cairo_rectangle (cairo_t               *cr,
                     const graphene_rect_t *rect)
{
  cairo_rectangle (cr,
                   rect->origin.x, rect->origin.y,
                   rect->size.width, rect->size.height);
}

static void
rectangle_init_from_graphene (cairo_rectangle_int_t *cairo,
                              const graphene_rect_t *graphene)
{
  cairo->x = floorf (graphene->origin.x);
  cairo->y = floorf (graphene->origin.y);
  cairo->width = ceilf (graphene->origin.x + graphene->size.width) - cairo->x;
  cairo->height = ceilf (graphene->origin.y + graphene->size.height) - cairo->y;
}

/*** GSK_COLOR_NODE ***/

/**
 * GskColorNode:
 *
 * A render node for a solid color.
 */
struct _GskColorNode
{
  GskRenderNode render_node;

  GdkRGBA color;
};

static void
gsk_color_node_draw (GskRenderNode *node,
                     cairo_t       *cr)
{
  GskColorNode *self = (GskColorNode *) node;

  gdk_cairo_set_source_rgba (cr, &self->color);

  gsk_cairo_rectangle (cr, &node->bounds);
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

/**
 * gsk_color_node_get_color:
 * @node: (type GskColorNode): a `GskRenderNode`
 *
 * Retrieves the color of the given @node.
 *
 * Returns: (transfer none): the color of the node
 */
const GdkRGBA *
gsk_color_node_get_color (const GskRenderNode *node)
{
  GskColorNode *self = (GskColorNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_COLOR_NODE), NULL);

  return &self->color;
}

/**
 * gsk_color_node_new:
 * @rgba: a `GdkRGBA` specifying a color
 * @bounds: the rectangle to render the color into
 *
 * Creates a `GskRenderNode` that will render the color specified by @rgba into
 * the area given by @bounds.
 *
 * Returns: (transfer full) (type GskColorNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_color_node_new (const GdkRGBA         *rgba,
                    const graphene_rect_t *bounds)
{
  GskColorNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (rgba != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  self = gsk_render_node_alloc (GSK_COLOR_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  self->color = *rgba;
  graphene_rect_init_from_rect (&node->bounds, bounds);

  return node;
}

/*** GSK_LINEAR_GRADIENT_NODE ***/

/**
 * GskRepeatingLinearGradientNode:
 *
 * A render node for a repeating linear gradient.
 */

/**
 * GskLinearGradientNode:
 *
 * A render node for a linear gradient.
 */
struct _GskLinearGradientNode
{
  GskRenderNode render_node;

  graphene_point_t start;
  graphene_point_t end;

  gsize n_stops;
  GskColorStop *stops;
};

static void
gsk_linear_gradient_node_finalize (GskRenderNode *node)
{
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_LINEAR_GRADIENT_NODE));

  g_free (self->stops);

  parent_class->finalize (node);
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

  gsk_cairo_rectangle (cr, &node->bounds);
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

/**
 * gsk_linear_gradient_node_new:
 * @bounds: the rectangle to render the linear gradient into
 * @start: the point at which the linear gradient will begin
 * @end: the point at which the linear gradient will finish
 * @color_stops: (array length=n_color_stops): a pointer to an array of
 *   `GskColorStop` defining the gradient. The offsets of all color stops
 *   must be increasing. The first stop's offset must be >= 0 and the last
 *   stop's offset must be <= 1.
 * @n_color_stops: the number of elements in @color_stops
 *
 * Creates a `GskRenderNode` that will create a linear gradient from the given
 * points and color stops, and render that into the area given by @bounds.
 *
 * Returns: (transfer full) (type GskLinearGradientNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_linear_gradient_node_new (const graphene_rect_t  *bounds,
                              const graphene_point_t *start,
                              const graphene_point_t *end,
                              const GskColorStop     *color_stops,
                              gsize                   n_color_stops)
{
  GskLinearGradientNode *self;
  GskRenderNode *node;
  gsize i;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);
  g_return_val_if_fail (color_stops[0].offset >= 0, NULL);
  for (i = 1; i < n_color_stops; i++)
    g_return_val_if_fail (color_stops[i].offset >= color_stops[i - 1].offset, NULL);
  g_return_val_if_fail (color_stops[n_color_stops - 1].offset <= 1, NULL);

  self = gsk_render_node_alloc (GSK_LINEAR_GRADIENT_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  graphene_rect_init_from_rect (&node->bounds, bounds);
  graphene_point_init_from_point (&self->start, start);
  graphene_point_init_from_point (&self->end, end);

  self->n_stops = n_color_stops;
  self->stops = g_malloc_n (n_color_stops, sizeof (GskColorStop));
  memcpy (self->stops, color_stops, n_color_stops * sizeof (GskColorStop));

  return node;
}

/**
 * gsk_repeating_linear_gradient_node_new:
 * @bounds: the rectangle to render the linear gradient into
 * @start: the point at which the linear gradient will begin
 * @end: the point at which the linear gradient will finish
 * @color_stops: (array length=n_color_stops): a pointer to an array of
 * `GskColorStop` defining the gradient. The offsets of all color stops
 *   must be increasing. The first stop's offset must be >= 0 and the last
 *   stop's offset must be <= 1.
 * @n_color_stops: the number of elements in @color_stops
 *
 * Creates a `GskRenderNode` that will create a repeating linear gradient
 * from the given points and color stops, and render that into the area
 * given by @bounds.
 *
 * Returns: (transfer full) (type GskRepeatingLinearGradientNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_repeating_linear_gradient_node_new (const graphene_rect_t  *bounds,
                                        const graphene_point_t *start,
                                        const graphene_point_t *end,
                                        const GskColorStop     *color_stops,
                                        gsize                   n_color_stops)
{
  GskLinearGradientNode *self;
  GskRenderNode *node;
  gsize i;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);
  g_return_val_if_fail (color_stops[0].offset >= 0, NULL);
  for (i = 1; i < n_color_stops; i++)
    g_return_val_if_fail (color_stops[i].offset >= color_stops[i - 1].offset, NULL);
  g_return_val_if_fail (color_stops[n_color_stops - 1].offset <= 1, NULL);

  self = gsk_render_node_alloc (GSK_REPEATING_LINEAR_GRADIENT_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  graphene_rect_init_from_rect (&node->bounds, bounds);
  graphene_point_init_from_point (&self->start, start);
  graphene_point_init_from_point (&self->end, end);

  self->stops = g_malloc_n (n_color_stops, sizeof (GskColorStop));
  memcpy (self->stops, color_stops, n_color_stops * sizeof (GskColorStop));
  self->n_stops = n_color_stops;

  return node;
}

/**
 * gsk_linear_gradient_node_get_start:
 * @node: (type GskLinearGradientNode): a `GskRenderNode` for a linear gradient
 *
 * Retrieves the initial point of the linear gradient.
 *
 * Returns: (transfer none): the initial point
 */
const graphene_point_t *
gsk_linear_gradient_node_get_start (const GskRenderNode *node)
{
  const GskLinearGradientNode *self = (const GskLinearGradientNode *) node;

  return &self->start;
}

/**
 * gsk_linear_gradient_node_get_end:
 * @node: (type GskLinearGradientNode): a `GskRenderNode` for a linear gradient
 *
 * Retrieves the final point of the linear gradient.
 *
 * Returns: (transfer none): the final point
 */
const graphene_point_t *
gsk_linear_gradient_node_get_end (const GskRenderNode *node)
{
  const GskLinearGradientNode *self = (const GskLinearGradientNode *) node;

  return &self->end;
}

/**
 * gsk_linear_gradient_node_get_n_color_stops:
 * @node: (type GskLinearGradientNode): a `GskRenderNode` for a linear gradient
 *
 * Retrieves the number of color stops in the gradient.
 *
 * Returns: the number of color stops
 */
gsize
gsk_linear_gradient_node_get_n_color_stops (const GskRenderNode *node)
{
  const GskLinearGradientNode *self = (const GskLinearGradientNode *) node;

  return self->n_stops;
}

/**
 * gsk_linear_gradient_node_get_color_stops:
 * @node: (type GskLinearGradientNode): a `GskRenderNode` for a linear gradient
 * @n_stops: (out) (optional): the number of color stops in the returned array
 *
 * Retrieves the color stops in the gradient.
 *
 * Returns: (array length=n_stops): the color stops in the gradient
 */
const GskColorStop *
gsk_linear_gradient_node_get_color_stops (const GskRenderNode *node,
                                          gsize               *n_stops)
{
  const GskLinearGradientNode *self = (const GskLinearGradientNode *) node;

  if (n_stops != NULL)
    *n_stops = self->n_stops;

  return self->stops;
}

/*** GSK_RADIAL_GRADIENT_NODE ***/

/**
 * GskRepeatingRadialGradientNode:
 *
 * A render node for a repeating radial gradient.
 */

/**
 * GskRadialGradientNode:
 *
 * A render node for a radial gradient.
 */
struct _GskRadialGradientNode
{
  GskRenderNode render_node;

  graphene_point_t center;

  float hradius;
  float vradius;
  float start;
  float end;

  gsize n_stops;
  GskColorStop *stops;
};

static void
gsk_radial_gradient_node_finalize (GskRenderNode *node)
{
  GskRadialGradientNode *self = (GskRadialGradientNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_RADIAL_GRADIENT_NODE));

  g_free (self->stops);

  parent_class->finalize (node);
}

static void
gsk_radial_gradient_node_draw (GskRenderNode *node,
                               cairo_t       *cr)
{
  GskRadialGradientNode *self = (GskRadialGradientNode *) node;
  cairo_pattern_t *pattern;
  gsize i;

  pattern = cairo_pattern_create_radial (0, 0, self->hradius * self->start,
                                         0, 0, self->hradius * self->end);

  if (self->hradius != self->vradius)
    {
      cairo_matrix_t matrix;

      cairo_matrix_init_scale (&matrix, 1.0, self->hradius / self->vradius);
      cairo_pattern_set_matrix (pattern, &matrix);
    }

  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE)
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  else
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  for (i = 0; i < self->n_stops; i++)
    cairo_pattern_add_color_stop_rgba (pattern,
                                       self->stops[i].offset,
                                       self->stops[i].color.red,
                                       self->stops[i].color.green,
                                       self->stops[i].color.blue,
                                       self->stops[i].color.alpha);

  gsk_cairo_rectangle (cr, &node->bounds);
  cairo_translate (cr, self->center.x, self->center.y);
  cairo_set_source (cr, pattern);
  cairo_fill (cr);

  cairo_pattern_destroy (pattern);
}

static void
gsk_radial_gradient_node_diff (GskRenderNode  *node1,
                               GskRenderNode  *node2,
                               cairo_region_t *region)
{
  GskRadialGradientNode *self1 = (GskRadialGradientNode *) node1;
  GskRadialGradientNode *self2 = (GskRadialGradientNode *) node2;

  if (graphene_point_equal (&self1->center, &self2->center) &&
      self1->hradius == self2->hradius &&
      self1->vradius == self2->vradius &&
      self1->start == self2->start &&
      self1->end == self2->end &&
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

/**
 * gsk_radial_gradient_node_new:
 * @bounds: the bounds of the node
 * @center: the center of the gradient
 * @hradius: the horizontal radius
 * @vradius: the vertical radius
 * @start: a percentage >= 0 that defines the start of the gradient around @center
 * @end: a percentage >= 0 that defines the end of the gradient around @center
 * @color_stops: (array length=n_color_stops): a pointer to an array of
 *   `GskColorStop` defining the gradient. The offsets of all color stops
 *   must be increasing. The first stop's offset must be >= 0 and the last
 *   stop's offset must be <= 1.
 * @n_color_stops: the number of elements in @color_stops
 *
 * Creates a `GskRenderNode` that draws a radial gradient.
 *
 * The radial gradient
 * starts around @center. The size of the gradient is dictated by @hradius
 * in horizontal orientation and by @vradius in vertical orientation.
 *
 * Returns: (transfer full) (type GskRadialGradientNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_radial_gradient_node_new (const graphene_rect_t  *bounds,
                              const graphene_point_t *center,
                              float                   hradius,
                              float                   vradius,
                              float                   start,
                              float                   end,
                              const GskColorStop     *color_stops,
                              gsize                   n_color_stops)
{
  GskRadialGradientNode *self;
  GskRenderNode *node;
  gsize i;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (center != NULL, NULL);
  g_return_val_if_fail (hradius > 0., NULL);
  g_return_val_if_fail (vradius > 0., NULL);
  g_return_val_if_fail (start >= 0., NULL);
  g_return_val_if_fail (end >= 0., NULL);
  g_return_val_if_fail (end > start, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);
  g_return_val_if_fail (color_stops[0].offset >= 0, NULL);
  for (i = 1; i < n_color_stops; i++)
    g_return_val_if_fail (color_stops[i].offset >= color_stops[i - 1].offset, NULL);
  g_return_val_if_fail (color_stops[n_color_stops - 1].offset <= 1, NULL);

  self = gsk_render_node_alloc (GSK_RADIAL_GRADIENT_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  graphene_rect_init_from_rect (&node->bounds, bounds);
  graphene_point_init_from_point (&self->center, center);

  self->hradius = hradius;
  self->vradius = vradius;
  self->start = start;
  self->end = end;

  self->n_stops = n_color_stops;
  self->stops = g_malloc_n (n_color_stops, sizeof (GskColorStop));
  memcpy (self->stops, color_stops, n_color_stops * sizeof (GskColorStop));

  return node;
}

/**
 * gsk_repeating_radial_gradient_node_new:
 * @bounds: the bounds of the node
 * @center: the center of the gradient
 * @hradius: the horizontal radius
 * @vradius: the vertical radius
 * @start: a percentage >= 0 that defines the start of the gradient around @center
 * @end: a percentage >= 0 that defines the end of the gradient around @center
 * @color_stops: (array length=n_color_stops): a pointer to an array of
 *   `GskColorStop` defining the gradient. The offsets of all color stops
 *   must be increasing. The first stop's offset must be >= 0 and the last
 *   stop's offset must be <= 1.
 * @n_color_stops: the number of elements in @color_stops
 *
 * Creates a `GskRenderNode` that draws a repeating radial gradient.
 *
 * The radial gradient starts around @center. The size of the gradient
 * is dictated by @hradius in horizontal orientation and by @vradius
 * in vertical orientation.
 *
 * Returns: (transfer full) (type GskRepeatingRadialGradientNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_repeating_radial_gradient_node_new (const graphene_rect_t  *bounds,
                                        const graphene_point_t *center,
                                        float                   hradius,
                                        float                   vradius,
                                        float                   start,
                                        float                   end,
                                        const GskColorStop     *color_stops,
                                        gsize                   n_color_stops)
{
  GskRadialGradientNode *self;
  GskRenderNode *node;
  gsize i;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (center != NULL, NULL);
  g_return_val_if_fail (hradius > 0., NULL);
  g_return_val_if_fail (vradius > 0., NULL);
  g_return_val_if_fail (start >= 0., NULL);
  g_return_val_if_fail (end >= 0., NULL);
  g_return_val_if_fail (end > start, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);
  g_return_val_if_fail (color_stops[0].offset >= 0, NULL);
  for (i = 1; i < n_color_stops; i++)
    g_return_val_if_fail (color_stops[i].offset >= color_stops[i - 1].offset, NULL);
  g_return_val_if_fail (color_stops[n_color_stops - 1].offset <= 1, NULL);

  self = gsk_render_node_alloc (GSK_REPEATING_RADIAL_GRADIENT_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  graphene_rect_init_from_rect (&node->bounds, bounds);
  graphene_point_init_from_point (&self->center, center);

  self->hradius = hradius;
  self->vradius = vradius;
  self->start = start;
  self->end = end;

  self->n_stops = n_color_stops;
  self->stops = g_malloc_n (n_color_stops, sizeof (GskColorStop));
  memcpy (self->stops, color_stops, n_color_stops * sizeof (GskColorStop));

  return node;
}

/**
 * gsk_radial_gradient_node_get_n_color_stops:
 * @node: (type GskRadialGradientNode): a `GskRenderNode` for a radial gradient
 *
 * Retrieves the number of color stops in the gradient.
 *
 * Returns: the number of color stops
 */
gsize
gsk_radial_gradient_node_get_n_color_stops (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return self->n_stops;
}

/**
 * gsk_radial_gradient_node_get_color_stops:
 * @node: (type GskRadialGradientNode): a `GskRenderNode` for a radial gradient
 * @n_stops: (out) (optional): the number of color stops in the returned array
 *
 * Retrieves the color stops in the gradient.
 *
 * Returns: (array length=n_stops): the color stops in the gradient
 */
const GskColorStop *
gsk_radial_gradient_node_get_color_stops (const GskRenderNode *node,
                                          gsize               *n_stops)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  if (n_stops != NULL)
    *n_stops = self->n_stops;

  return self->stops;
}

/**
 * gsk_radial_gradient_node_get_center:
 * @node: (type GskRadialGradientNode): a `GskRenderNode` for a radial gradient
 *
 * Retrieves the center pointer for the gradient.
 *
 * Returns: the center point for the gradient
 */
const graphene_point_t *
gsk_radial_gradient_node_get_center (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return &self->center;
}

/**
 * gsk_radial_gradient_node_get_hradius:
 * @node: (type GskRadialGradientNode): a `GskRenderNode` for a radial gradient
 *
 * Retrieves the horizontal radius for the gradient.
 *
 * Returns: the horizontal radius for the gradient
 */
float
gsk_radial_gradient_node_get_hradius (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return self->hradius;
}

/**
 * gsk_radial_gradient_node_get_vradius:
 * @node: (type GskRadialGradientNode): a `GskRenderNode` for a radial gradient
 *
 * Retrieves the vertical radius for the gradient.
 *
 * Returns: the vertical radius for the gradient
 */
float
gsk_radial_gradient_node_get_vradius (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return self->vradius;
}

/**
 * gsk_radial_gradient_node_get_start:
 * @node: (type GskRadialGradientNode): a `GskRenderNode` for a radial gradient
 *
 * Retrieves the start value for the gradient.
 *
 * Returns: the start value for the gradient
 */
float
gsk_radial_gradient_node_get_start (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return self->start;
}

/**
 * gsk_radial_gradient_node_get_end:
 * @node: (type GskRadialGradientNode): a `GskRenderNode` for a radial gradient
 *
 * Retrieves the end value for the gradient.
 *
 * Returns: the end value for the gradient
 */
float
gsk_radial_gradient_node_get_end (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return self->end;
}

/*** GSK_CONIC_GRADIENT_NODE ***/

/**
 * GskConicGradientNode:
 *
 * A render node for a conic gradient.
 */

struct _GskConicGradientNode
{
  GskRenderNode render_node;

  graphene_point_t center;
  float rotation;
  float angle;

  gsize n_stops;
  GskColorStop *stops;
};

static void
gsk_conic_gradient_node_finalize (GskRenderNode *node)
{
  GskConicGradientNode *self = (GskConicGradientNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CONIC_GRADIENT_NODE));

  g_free (self->stops);

  parent_class->finalize (node);
}

#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))

static void
_cairo_mesh_pattern_set_corner_rgba (cairo_pattern_t *pattern,
                                     guint            corner_num,
                                     const GdkRGBA   *rgba)
{
  cairo_mesh_pattern_set_corner_color_rgba (pattern, corner_num, rgba->red, rgba->green, rgba->blue, rgba->alpha);
}

static void
project (double  angle,
         double  radius,
         double *x_out,
         double *y_out)
{
  double x, y;

#ifdef HAVE_SINCOS
  sincos (angle, &y, &x);
#else
  x = cos (angle);
  y = sin (angle);
#endif
  *x_out = radius * x;
  *y_out = radius * y;
}

static void
gsk_conic_gradient_node_add_patch (cairo_pattern_t *pattern,
                                   float            radius,
                                   float            start_angle,
                                   const GdkRGBA   *start_color,
                                   float            end_angle,
                                   const GdkRGBA   *end_color)
{
  double x, y;

  cairo_mesh_pattern_begin_patch (pattern);

  cairo_mesh_pattern_move_to  (pattern, 0, 0);
  project (start_angle, radius, &x, &y);
  cairo_mesh_pattern_line_to  (pattern, x, y);
  project (end_angle, radius, &x, &y);
  cairo_mesh_pattern_line_to  (pattern, x, y);
  cairo_mesh_pattern_line_to  (pattern, 0, 0);

  _cairo_mesh_pattern_set_corner_rgba (pattern, 0, start_color);
  _cairo_mesh_pattern_set_corner_rgba (pattern, 1, start_color);
  _cairo_mesh_pattern_set_corner_rgba (pattern, 2, end_color);
  _cairo_mesh_pattern_set_corner_rgba (pattern, 3, end_color);

  cairo_mesh_pattern_end_patch (pattern);
}

static void
gdk_rgba_color_interpolate (GdkRGBA       *dest,
                            const GdkRGBA *src1,
                            const GdkRGBA *src2,
                            double         progress)
{
  double alpha = src1->alpha * (1.0 - progress) + src2->alpha * progress;

  dest->alpha = alpha;
  if (alpha == 0)
    {
      dest->red = src1->red * (1.0 - progress) + src2->red * progress;
      dest->green = src1->green * (1.0 - progress) + src2->green * progress;
      dest->blue = src1->blue * (1.0 - progress) + src2->blue * progress;
    }
  else
    {
      dest->red = (src1->red * src1->alpha * (1.0 - progress) + src2->red * src2->alpha * progress) / alpha;
      dest->green = (src1->green * src1->alpha * (1.0 - progress) + src2->green * src2->alpha * progress) / alpha;
      dest->blue = (src1->blue * src1->alpha * (1.0 - progress) + src2->blue * src2->alpha * progress) / alpha;
    }
}

static void
gsk_conic_gradient_node_draw (GskRenderNode *node,
                              cairo_t       *cr)
{
  GskConicGradientNode *self = (GskConicGradientNode *) node;
  cairo_pattern_t *pattern;
  graphene_point_t corner;
  float radius;
  gsize i;

  pattern = cairo_pattern_create_mesh ();
  graphene_rect_get_top_right (&node->bounds, &corner);
  radius = graphene_point_distance (&self->center, &corner, NULL, NULL);
  graphene_rect_get_bottom_right (&node->bounds, &corner);
  radius = MAX (radius, graphene_point_distance (&self->center, &corner, NULL, NULL));
  graphene_rect_get_bottom_left (&node->bounds, &corner);
  radius = MAX (radius, graphene_point_distance (&self->center, &corner, NULL, NULL));
  graphene_rect_get_top_left (&node->bounds, &corner);
  radius = MAX (radius, graphene_point_distance (&self->center, &corner, NULL, NULL));

  for (i = 0; i <= self->n_stops; i++)
    {
      GskColorStop *stop1 = &self->stops[MAX (i, 1) - 1];
      GskColorStop *stop2 = &self->stops[MIN (i, self->n_stops - 1)];
      double offset1 = i > 0 ? stop1->offset : 0;
      double offset2 = i < self->n_stops ? stop2->offset : 1;
      double start_angle, end_angle;

      offset1 = offset1 * 360 + self->rotation - 90;
      offset2 = offset2 * 360 + self->rotation - 90;

      for (start_angle = offset1; start_angle < offset2; start_angle = end_angle)
        {
          GdkRGBA start_color, end_color;
          end_angle = (floor (start_angle / 45) + 1) * 45;
          end_angle = MIN (end_angle, offset2);
          gdk_rgba_color_interpolate (&start_color,
                                      &stop1->color,
                                      &stop2->color,
                                      (start_angle - offset1) / (offset2 - offset1));
          gdk_rgba_color_interpolate (&end_color,
                                      &stop1->color,
                                      &stop2->color,
                                      (end_angle - offset1) / (offset2 - offset1));

          gsk_conic_gradient_node_add_patch (pattern,
                                             radius,
                                             DEG_TO_RAD (start_angle),
                                             &start_color,
                                             DEG_TO_RAD (end_angle),
                                             &end_color);
        }
    }

  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  gsk_cairo_rectangle (cr, &node->bounds);
  cairo_translate (cr, self->center.x, self->center.y);
  cairo_set_source (cr, pattern);
  cairo_fill (cr);

  cairo_pattern_destroy (pattern);
}

static void
gsk_conic_gradient_node_diff (GskRenderNode  *node1,
                              GskRenderNode  *node2,
                              cairo_region_t *region)
{
  GskConicGradientNode *self1 = (GskConicGradientNode *) node1;
  GskConicGradientNode *self2 = (GskConicGradientNode *) node2;
  gsize i;

  if (!graphene_point_equal (&self1->center, &self2->center) ||
      self1->rotation != self2->rotation ||
      self1->n_stops != self2->n_stops)
    {
      gsk_render_node_diff_impossible (node1, node2, region);
      return;
    }

  for (i = 0; i < self1->n_stops; i++)
    {
      GskColorStop *stop1 = &self1->stops[i];
      GskColorStop *stop2 = &self2->stops[i];

      if (stop1->offset != stop2->offset ||
          !gdk_rgba_equal (&stop1->color, &stop2->color))
        {
          gsk_render_node_diff_impossible (node1, node2, region);
          return;
        }
    }
}

/**
 * gsk_conic_gradient_node_new:
 * @bounds: the bounds of the node
 * @center: the center of the gradient
 * @rotation: the rotation of the gradient in degrees
 * @color_stops: (array length=n_color_stops): a pointer to an array of
 *   `GskColorStop` defining the gradient. The offsets of all color stops
 *   must be increasing. The first stop's offset must be >= 0 and the last
 *   stop's offset must be <= 1.
 * @n_color_stops: the number of elements in @color_stops
 *
 * Creates a `GskRenderNode` that draws a conic gradient.
 *
 * The conic gradient
 * starts around @center in the direction of @rotation. A rotation of 0 means
 * that the gradient points up. Color stops are then added clockwise.
 *
 * Returns: (transfer full) (type GskConicGradientNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_conic_gradient_node_new (const graphene_rect_t  *bounds,
                             const graphene_point_t *center,
                             float                   rotation,
                             const GskColorStop     *color_stops,
                             gsize                   n_color_stops)
{
  GskConicGradientNode *self;
  GskRenderNode *node;
  gsize i;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (center != NULL, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);
  g_return_val_if_fail (color_stops[0].offset >= 0, NULL);
  for (i = 1; i < n_color_stops; i++)
    g_return_val_if_fail (color_stops[i].offset >= color_stops[i - 1].offset, NULL);
  g_return_val_if_fail (color_stops[n_color_stops - 1].offset <= 1, NULL);

  self = gsk_render_node_alloc (GSK_CONIC_GRADIENT_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  graphene_rect_init_from_rect (&node->bounds, bounds);
  graphene_point_init_from_point (&self->center, center);

  self->rotation = rotation;

  self->n_stops = n_color_stops;
  self->stops = g_malloc_n (n_color_stops, sizeof (GskColorStop));
  memcpy (self->stops, color_stops, n_color_stops * sizeof (GskColorStop));

  self->angle = 90.f - self->rotation;
  self->angle = G_PI * self->angle / 180.f;
  self->angle = fmodf (self->angle, 2.f * G_PI);
  if (self->angle < 0.f)
    self->angle += 2.f * G_PI;

  return node;
}

/**
 * gsk_conic_gradient_node_get_n_color_stops:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 *
 * Retrieves the number of color stops in the gradient.
 *
 * Returns: the number of color stops
 */
gsize
gsk_conic_gradient_node_get_n_color_stops (const GskRenderNode *node)
{
  const GskConicGradientNode *self = (const GskConicGradientNode *) node;

  return self->n_stops;
}

/**
 * gsk_conic_gradient_node_get_color_stops:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 * @n_stops: (out) (optional): the number of color stops in the returned array
 *
 * Retrieves the color stops in the gradient.
 *
 * Returns: (array length=n_stops): the color stops in the gradient
 */
const GskColorStop *
gsk_conic_gradient_node_get_color_stops (const GskRenderNode *node,
                                         gsize               *n_stops)
{
  const GskConicGradientNode *self = (const GskConicGradientNode *) node;

  if (n_stops != NULL)
    *n_stops = self->n_stops;

  return self->stops;
}

/**
 * gsk_conic_gradient_node_get_center:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 *
 * Retrieves the center pointer for the gradient.
 *
 * Returns: the center point for the gradient
 */
const graphene_point_t *
gsk_conic_gradient_node_get_center (const GskRenderNode *node)
{
  const GskConicGradientNode *self = (const GskConicGradientNode *) node;

  return &self->center;
}

/**
 * gsk_conic_gradient_node_get_rotation:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 *
 * Retrieves the rotation for the gradient in degrees.
 *
 * Returns: the rotation for the gradient
 */
float
gsk_conic_gradient_node_get_rotation (const GskRenderNode *node)
{
  const GskConicGradientNode *self = (const GskConicGradientNode *) node;

  return self->rotation;
}

/**
 * gsk_conic_gradient_node_get_angle:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 *
 * Retrieves the angle for the gradient in radians, normalized in [0, 2 * PI].
 *
 * The angle is starting at the top and going clockwise, as expressed
 * in the css specification:
 *
 *     angle = 90 - gsk_conic_gradient_node_get_rotation()
 *
 * Returns: the angle for the gradient
 *
 * Since: 4.2
 */
float
gsk_conic_gradient_node_get_angle (const GskRenderNode *node)
{
  const GskConicGradientNode *self = (const GskConicGradientNode *) node;

  return self->angle;
}

/*** GSK_BORDER_NODE ***/

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
  GdkRGBA border_color[4];
};

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
  gboolean uniform1 = self1->uniform_width && self1->uniform_color;
  gboolean uniform2 = self2->uniform_width && self2->uniform_color;

  if (uniform1 &&
      uniform2 &&
      self1->border_width[0] == self2->border_width[0] &&
      gsk_rounded_rect_equal (&self1->outline, &self2->outline) &&
      gdk_rgba_equal (&self1->border_color[0], &self2->border_color[0]))
    return;

  /* Different uniformity -> diff impossible */
  if (uniform1 ^ uniform2)
    {
      gsk_render_node_diff_impossible (node1, node2, region);
      return;
    }

  if (self1->border_width[0] == self2->border_width[0] &&
      self1->border_width[1] == self2->border_width[1] &&
      self1->border_width[2] == self2->border_width[2] &&
      self1->border_width[3] == self2->border_width[3] &&
      gdk_rgba_equal (&self1->border_color[0], &self2->border_color[0]) &&
      gdk_rgba_equal (&self1->border_color[1], &self2->border_color[1]) &&
      gdk_rgba_equal (&self1->border_color[2], &self2->border_color[2]) &&
      gdk_rgba_equal (&self1->border_color[3], &self2->border_color[3]) &&
      gsk_rounded_rect_equal (&self1->outline, &self2->outline))
    return;

  gsk_render_node_diff_impossible (node1, node2, region);
}

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
 * Returns: (transfer none): an array of 4 `GdkRGBA` structs
 *     for the top, right, bottom and left color of the border
 */
const GdkRGBA *
gsk_border_node_get_colors (const GskRenderNode *node)
{
  const GskBorderNode *self = (const GskBorderNode *) node;

  return self->border_color;
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
  GskBorderNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (border_width != NULL, NULL);
  g_return_val_if_fail (border_color != NULL, NULL);

  self = gsk_render_node_alloc (GSK_BORDER_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  gsk_rounded_rect_init_copy (&self->outline, outline);
  memcpy (self->border_width, border_width, sizeof (self->border_width));
  memcpy (self->border_color, border_color, sizeof (self->border_color));

  if (border_width[0] == border_width[1] &&
      border_width[0] == border_width[2] &&
      border_width[0] == border_width[3])
    self->uniform_width = TRUE;
  else
    self->uniform_width = FALSE;

  if (gdk_rgba_equal (&border_color[0], &border_color[1]) &&
      gdk_rgba_equal (&border_color[0], &border_color[2]) &&
      gdk_rgba_equal (&border_color[0], &border_color[3]))
    self->uniform_color = TRUE;
  else
    self->uniform_color = FALSE;

  graphene_rect_init_from_rect (&node->bounds, &self->outline.bounds);

  return node;
}

/* Private */
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

/*** GSK_TEXTURE_NODE ***/

/**
 * GskTextureNode:
 *
 * A render node for a `GdkTexture`.
 */
struct _GskTextureNode
{
  GskRenderNode render_node;

  GdkTexture *texture;
};

static void
gsk_texture_node_finalize (GskRenderNode *node)
{
  GskTextureNode *self = (GskTextureNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_TEXTURE_NODE));

  g_clear_object (&self->texture);

  parent_class->finalize (node);
}

static void
gsk_texture_node_draw (GskRenderNode *node,
                       cairo_t       *cr)
{
  GskTextureNode *self = (GskTextureNode *) node;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;

  surface = gdk_texture_download_surface (self->texture);
  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  cairo_matrix_init_scale (&matrix,
                           gdk_texture_get_width (self->texture) / node->bounds.size.width,
                           gdk_texture_get_height (self->texture) / node->bounds.size.height);
  cairo_matrix_translate (&matrix,
                          -node->bounds.origin.x,
                          -node->bounds.origin.y);
  cairo_pattern_set_matrix (pattern, &matrix);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);

  gsk_cairo_rectangle (cr, &node->bounds);
  cairo_fill (cr);
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

/**
 * gsk_texture_node_get_texture:
 * @node: (type GskTextureNode): a `GskRenderNode` of type %GSK_TEXTURE_NODE
 *
 * Retrieves the `GdkTexture` used when creating this `GskRenderNode`.
 *
 * Returns: (transfer none): the `GdkTexture`
 */
GdkTexture *
gsk_texture_node_get_texture (const GskRenderNode *node)
{
  const GskTextureNode *self = (const GskTextureNode *) node;

  return self->texture;
}

/**
 * gsk_texture_node_new:
 * @texture: the `GdkTexture`
 * @bounds: the rectangle to render the texture into
 *
 * Creates a `GskRenderNode` that will render the given
 * @texture into the area given by @bounds.
 *
 * Returns: (transfer full) (type GskTextureNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_texture_node_new (GdkTexture            *texture,
                      const graphene_rect_t *bounds)
{
  GskTextureNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TEXTURE_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  self->texture = g_object_ref (texture);
  graphene_rect_init_from_rect (&node->bounds, bounds);

  node->prefers_high_depth = gdk_memory_format_prefers_high_depth (gdk_texture_get_format (texture));

  return node;
}

/*** GSK_INSET_SHADOW_NODE ***/

/**
 * GskInsetShadowNode:
 *
 * A render node for an inset shadow.
 */
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

static gboolean
has_empty_clip (cairo_t *cr)
{
  double x1, y1, x2, y2;

  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
  return x1 == x2 && y1 == y2;
}

static void
draw_shadow (cairo_t              *cr,
             gboolean              inset,
             const GskRoundedRect *box,
             const GskRoundedRect *clip_box,
             float                 radius,
             const GdkRGBA        *color,
             GskBlurFlags          blur_flags)
{
  cairo_t *shadow_cr;

  if (has_empty_clip (cr))
    return;

  gdk_cairo_set_source_rgba (cr, color);
  shadow_cr = gsk_cairo_blur_start_drawing (cr, radius, blur_flags);

  cairo_set_fill_rule (shadow_cr, CAIRO_FILL_RULE_EVEN_ODD);
  gsk_rounded_rect_path (box, shadow_cr);
  if (inset)
    gsk_cairo_rectangle (shadow_cr, &clip_box->bounds);

  cairo_fill (shadow_cr);

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
   * The horizontal and vertical corner radius
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
      g_hash_table_insert (corner_mask_cache, g_memdup2 (&key, sizeof (key)), mask);
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
  double clip_radius;
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
  double blur_radius;

  /* We don't need to draw invisible shadows */
  if (gdk_rgba_is_clear (&self->color))
    return;

  cairo_clip_extents (cr, &x1c, &y1c, &x2c, &y2c);
  if (!gsk_rounded_rect_intersects_rect (&self->outline, &GRAPHENE_RECT_INIT (x1c, y1c, x2c - x1c, y2c - y1c)))
    return;

  blur_radius = self->blur_radius / 2;

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius);

  cairo_save (cr);

  gsk_rounded_rect_path (&self->outline, cr);
  cairo_clip (cr);

  gsk_rounded_rect_init_copy (&box, &self->outline);
  gsk_rounded_rect_offset (&box, self->dx, self->dy);
  gsk_rounded_rect_shrink (&box, self->spread, self->spread, self->spread, self->spread);

  gsk_rounded_rect_init_copy (&clip_box, &self->outline);
  gsk_rounded_rect_shrink (&clip_box, -clip_radius, -clip_radius, -clip_radius, -clip_radius);

  if (!needs_blur (blur_radius))
    draw_shadow (cr, TRUE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
  else
    {
      cairo_region_t *remaining;
      cairo_rectangle_int_t r;
      int i;

      /* For the blurred case we divide the rendering into 9 parts,
       * 4 of the corners, 4 for the horizonat/vertical lines and
       * one for the interior. We make the non-interior parts
       * large enough to fit the full radius of the blur, so that
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
          draw_shadow_corner (cr, TRUE, &box, &clip_box, blur_radius, &self->color, i, &r);
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
          draw_shadow_side (cr, TRUE, &box, &clip_box, blur_radius, &self->color, i, &r);
          cairo_restore (cr);

          /* We drew the region, remove it from remaining */
          cairo_region_subtract_rectangle (remaining, &r);
        }

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      draw_shadow (cr, TRUE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
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

/**
 * gsk_inset_shadow_node_new:
 * @outline: outline of the region containing the shadow
 * @color: color of the shadow
 * @dx: horizontal offset of shadow
 * @dy: vertical offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a `GskRenderNode` that will render an inset shadow
 * into the box given by @outline.
 *
 * Returns: (transfer full) (type GskInsetShadowNode): A new `GskRenderNode`
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
  GskRenderNode *node;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (color != NULL, NULL);

  self = gsk_render_node_alloc (GSK_INSET_SHADOW_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  gsk_rounded_rect_init_copy (&self->outline, outline);
  self->color = *color;
  self->dx = dx;
  self->dy = dy;
  self->spread = spread;
  self->blur_radius = blur_radius;

  graphene_rect_init_from_rect (&node->bounds, &self->outline.bounds);

  return node;
}

/**
 * gsk_inset_shadow_node_get_outline:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the outline rectangle of the inset shadow.
 *
 * Returns: (transfer none): a rounded rectangle
 */
const GskRoundedRect *
gsk_inset_shadow_node_get_outline (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return &self->outline;
}

/**
 * gsk_inset_shadow_node_get_color:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the color of the inset shadow.
 *
 * Returns: (transfer none): the color of the shadow
 */
const GdkRGBA *
gsk_inset_shadow_node_get_color (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return &self->color;
}

/**
 * gsk_inset_shadow_node_get_dx:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the horizontal offset of the inset shadow.
 *
 * Returns: an offset, in pixels
 */
float
gsk_inset_shadow_node_get_dx (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return self->dx;
}

/**
 * gsk_inset_shadow_node_get_dy:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the vertical offset of the inset shadow.
 *
 * Returns: an offset, in pixels
 */
float
gsk_inset_shadow_node_get_dy (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return self->dy;
}

/**
 * gsk_inset_shadow_node_get_spread:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves how much the shadow spreads inwards.
 *
 * Returns: the size of the shadow, in pixels
 */
float
gsk_inset_shadow_node_get_spread (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return self->spread;
}

/**
 * gsk_inset_shadow_node_get_blur_radius:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the blur radius to apply to the shadow.
 *
 * Returns: the blur radius, in pixels
 */
float
gsk_inset_shadow_node_get_blur_radius (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return self->blur_radius;
}

/*** GSK_OUTSET_SHADOW_NODE ***/

/**
 * GskOutsetShadowNode:
 *
 * A render node for an outset shadow.
 */
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
gsk_outset_shadow_get_extents (GskOutsetShadowNode *self,
                               float               *top,
                               float               *right,
                               float               *bottom,
                               float               *left)
{
  float clip_radius;

  clip_radius = gsk_cairo_blur_compute_pixels (ceil (self->blur_radius / 2.0));
  *top = MAX (0, ceil (clip_radius + self->spread - self->dy));
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
  double blur_radius;

  /* We don't need to draw invisible shadows */
  if (gdk_rgba_is_clear (&self->color))
    return;

  cairo_clip_extents (cr, &x1c, &y1c, &x2c, &y2c);
  if (gsk_rounded_rect_contains_rect (&self->outline, &GRAPHENE_RECT_INIT (x1c, y1c, x2c - x1c, y2c - y1c)))
    return;

  blur_radius = self->blur_radius / 2;

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius);

  cairo_save (cr);

  gsk_rounded_rect_init_copy (&clip_box, &self->outline);
  gsk_outset_shadow_get_extents (self, &top, &right, &bottom, &left);
  gsk_rounded_rect_shrink (&clip_box, -top, -right, -bottom, -left);

  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
  gsk_rounded_rect_path (&self->outline, cr);
  gsk_cairo_rectangle (cr, &clip_box.bounds);

  cairo_clip (cr);

  gsk_rounded_rect_init_copy (&box, &self->outline);
  gsk_rounded_rect_offset (&box, self->dx, self->dy);
  gsk_rounded_rect_shrink (&box, -self->spread, -self->spread, -self->spread, -self->spread);

  if (!needs_blur (blur_radius))
    draw_shadow (cr, FALSE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
  else
    {
      int i;
      cairo_region_t *remaining;
      cairo_rectangle_int_t r;

      /* For the blurred case we divide the rendering into 9 parts,
       * 4 of the corners, 4 for the horizonat/vertical lines and
       * one for the interior. We make the non-interior parts
       * large enough to fit the full radius of the blur, so that
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
          draw_shadow_corner (cr, FALSE, &box, &clip_box, blur_radius, &self->color, i, &r);
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
          draw_shadow_side (cr, FALSE, &box, &clip_box, blur_radius, &self->color, i, &r);
          cairo_restore (cr);

          /* We drew the region, remove it from remaining */
          cairo_region_subtract_rectangle (remaining, &r);
        }

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      draw_shadow (cr, FALSE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
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

/**
 * gsk_outset_shadow_node_new:
 * @outline: outline of the region surrounded by shadow
 * @color: color of the shadow
 * @dx: horizontal offset of shadow
 * @dy: vertical offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a `GskRenderNode` that will render an outset shadow
 * around the box given by @outline.
 *
 * Returns: (transfer full) (type GskOutsetShadowNode): A new `GskRenderNode`
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
  GskRenderNode *node;
  float top, right, bottom, left;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (color != NULL, NULL);

  self = gsk_render_node_alloc (GSK_OUTSET_SHADOW_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  gsk_rounded_rect_init_copy (&self->outline, outline);
  self->color = *color;
  self->dx = dx;
  self->dy = dy;
  self->spread = spread;
  self->blur_radius = blur_radius;

  gsk_outset_shadow_get_extents (self, &top, &right, &bottom, &left);

  graphene_rect_init_from_rect (&node->bounds, &self->outline.bounds);
  node->bounds.origin.x -= left;
  node->bounds.origin.y -= top;
  node->bounds.size.width += left + right;
  node->bounds.size.height += top + bottom;

  return node;
}

/**
 * gsk_outset_shadow_node_get_outline:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the outline rectangle of the outset shadow.
 *
 * Returns: (transfer none): a rounded rectangle
 */
const GskRoundedRect *
gsk_outset_shadow_node_get_outline (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return &self->outline;
}

/**
 * gsk_outset_shadow_node_get_color:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the color of the outset shadow.
 *
 * Returns: (transfer none): a color
 */
const GdkRGBA *
gsk_outset_shadow_node_get_color (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return &self->color;
}

/**
 * gsk_outset_shadow_node_get_dx:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the horizontal offset of the outset shadow.
 *
 * Returns: an offset, in pixels
 */
float
gsk_outset_shadow_node_get_dx (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return self->dx;
}

/**
 * gsk_outset_shadow_node_get_dy:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the vertical offset of the outset shadow.
 *
 * Returns: an offset, in pixels
 */
float
gsk_outset_shadow_node_get_dy (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return self->dy;
}

/**
 * gsk_outset_shadow_node_get_spread:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves how much the shadow spreads outwards.
 *
 * Returns: the size of the shadow, in pixels
 */
float
gsk_outset_shadow_node_get_spread (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return self->spread;
}

/**
 * gsk_outset_shadow_node_get_blur_radius:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the blur radius of the shadow.
 *
 * Returns: the blur radius, in pixels
 */
float
gsk_outset_shadow_node_get_blur_radius (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return self->blur_radius;
}

/*** GSK_CAIRO_NODE ***/

/**
 * GskCairoNode:
 *
 * A render node for a Cairo surface.
 */
struct _GskCairoNode
{
  GskRenderNode render_node;

  cairo_surface_t *surface;
};

static void
gsk_cairo_node_finalize (GskRenderNode *node)
{
  GskCairoNode *self = (GskCairoNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CAIRO_NODE));

  if (self->surface)
    cairo_surface_destroy (self->surface);

  parent_class->finalize (node);
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

/**
 * gsk_cairo_node_get_surface:
 * @node: (type GskCairoNode): a `GskRenderNode` for a Cairo surface
 *
 * Retrieves the Cairo surface used by the render node.
 *
 * Returns: (transfer none): a Cairo surface
 */
cairo_surface_t *
gsk_cairo_node_get_surface (GskRenderNode *node)
{
  GskCairoNode *self = (GskCairoNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CAIRO_NODE), NULL);

  return self->surface;
}

/**
 * gsk_cairo_node_new:
 * @bounds: the rectangle to render to
 *
 * Creates a `GskRenderNode` that will render a cairo surface
 * into the area given by @bounds.
 *
 * You can draw to the cairo surface using [method@Gsk.CairoNode.get_draw_context].
 *
 * Returns: (transfer full) (type GskCairoNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_cairo_node_new (const graphene_rect_t *bounds)
{
  GskCairoNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);

  self = gsk_render_node_alloc (GSK_CAIRO_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  graphene_rect_init_from_rect (&node->bounds, bounds);

  return node;
}

/**
 * gsk_cairo_node_get_draw_context:
 * @node: (type GskCairoNode): a `GskRenderNode` for a Cairo surface
 *
 * Creates a Cairo context for drawing using the surface associated
 * to the render node.
 *
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

  gsk_cairo_rectangle (res, &node->bounds);
  cairo_clip (res);

  return res;
}

/**** GSK_CONTAINER_NODE ***/

/**
 * GskContainerNode:
 *
 * A render node that can contain other render nodes.
 */
struct _GskContainerNode
{
  GskRenderNode render_node;

  gboolean disjoint;
  guint n_children;
  GskRenderNode **children;
};

static void
gsk_container_node_finalize (GskRenderNode *node)
{
  GskContainerNode *container = (GskContainerNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CONTAINER_NODE));

  for (guint i = 0; i < container->n_children; i++)
    gsk_render_node_unref (container->children[i]);

  g_free (container->children);

  parent_class->finalize (node);
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

static int
gsk_container_node_compare_func (gconstpointer elem1, gconstpointer elem2, gpointer data)
{
  return gsk_render_node_can_diff ((const GskRenderNode *) elem1, (const GskRenderNode *) elem2) ? 0 : 1;
}

static GskDiffResult
gsk_container_node_keep_func (gconstpointer elem1, gconstpointer elem2, gpointer data)
{
  gsk_render_node_diff ((GskRenderNode *) elem1, (GskRenderNode *) elem2, data);
  if (cairo_region_num_rectangles (data) > MAX_RECTS_IN_DIFF)
    return GSK_DIFF_ABORTED;

  return GSK_DIFF_OK;
}

static GskDiffResult
gsk_container_node_change_func (gconstpointer elem, gsize idx, gpointer data)
{
  const GskRenderNode *node = elem;
  cairo_region_t *region = data;
  cairo_rectangle_int_t rect;

  rectangle_init_from_graphene (&rect, &node->bounds);
  cairo_region_union_rectangle (region, &rect);
  if (cairo_region_num_rectangles (region) > MAX_RECTS_IN_DIFF)
    return GSK_DIFF_ABORTED;

  return GSK_DIFF_OK;
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

static gboolean
gsk_render_node_diff_multiple (GskRenderNode **nodes1,
                               gsize           n_nodes1,
                               GskRenderNode **nodes2,
                               gsize           n_nodes2,
                               cairo_region_t *region)
{
  return gsk_diff ((gconstpointer *) nodes1, n_nodes1,
                   (gconstpointer *) nodes2, n_nodes2,
                   gsk_container_node_get_diff_settings (),
                   region) == GSK_DIFF_OK;
}

void
gsk_container_node_diff_with (GskRenderNode   *container,
                              GskRenderNode   *other,
                              cairo_region_t  *region)
{
  GskContainerNode *self = (GskContainerNode *) container;

  if (gsk_render_node_diff_multiple (self->children,
                                     self->n_children,
                                     &other,
                                     1,
                                     region))
    return;

  gsk_render_node_diff_impossible (container, other, region);
}

static void
gsk_container_node_diff (GskRenderNode  *node1,
                         GskRenderNode  *node2,
                         cairo_region_t *region)
{
  GskContainerNode *self1 = (GskContainerNode *) node1;
  GskContainerNode *self2 = (GskContainerNode *) node2;

  if (gsk_render_node_diff_multiple (self1->children,
                                     self1->n_children,
                                     self2->children,
                                     self2->n_children,
                                     region))
    return;

  gsk_render_node_diff_impossible (node1, node2, region);
}

/**
 * gsk_container_node_new:
 * @children: (array length=n_children) (transfer none): The children of the node
 * @n_children: Number of children in the @children array
 *
 * Creates a new `GskRenderNode` instance for holding the given @children.
 *
 * The new node will acquire a reference to each of the children.
 *
 * Returns: (transfer full) (type GskContainerNode): the new `GskRenderNode`
 */
GskRenderNode *
gsk_container_node_new (GskRenderNode **children,
                        guint           n_children)
{
  GskContainerNode *self;
  GskRenderNode *node;

  self = gsk_render_node_alloc (GSK_CONTAINER_NODE);
  node = (GskRenderNode *) self;

  self->disjoint = TRUE;
  self->n_children = n_children;

  if (n_children == 0)
    {
      graphene_rect_init_from_rect (&node->bounds, graphene_rect_zero ());
    }
  else
    {
      graphene_rect_t bounds;

      self->children = g_malloc_n (n_children, sizeof (GskRenderNode *));

      self->children[0] = gsk_render_node_ref (children[0]);
      graphene_rect_init_from_rect (&bounds, &(children[0]->bounds));
      node->prefers_high_depth = gsk_render_node_prefers_high_depth (children[0]);

      for (guint i = 1; i < n_children; i++)
        {
          self->children[i] = gsk_render_node_ref (children[i]);
          self->disjoint &= !graphene_rect_intersection (&bounds, &(children[i]->bounds), NULL);
          graphene_rect_union (&bounds, &(children[i]->bounds), &bounds);
          node->prefers_high_depth |= gsk_render_node_prefers_high_depth (children[i]);
          node->offscreen_for_opacity |= children[i]->offscreen_for_opacity;
        }

      graphene_rect_init_from_rect (&node->bounds, &bounds);
      node->offscreen_for_opacity |= !self->disjoint;
    }

  return node;
}

/**
 * gsk_container_node_get_n_children:
 * @node: (type GskContainerNode): a container `GskRenderNode`
 *
 * Retrieves the number of direct children of @node.
 *
 * Returns: the number of children of the `GskRenderNode`
 */
guint
gsk_container_node_get_n_children (const GskRenderNode *node)
{
  const GskContainerNode *self = (const GskContainerNode *) node;

  return self->n_children;
}

/**
 * gsk_container_node_get_child:
 * @node: (type GskContainerNode): a container `GskRenderNode`
 * @idx: the position of the child to get
 *
 * Gets one of the children of @container.
 *
 * Returns: (transfer none): the @idx'th child of @container
 */
GskRenderNode *
gsk_container_node_get_child (const GskRenderNode *node,
                              guint                idx)
{
  const GskContainerNode *self = (const GskContainerNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE), NULL);
  g_return_val_if_fail (idx < self->n_children, NULL);

  return self->children[idx];
}

GskRenderNode **
gsk_container_node_get_children (const GskRenderNode *node,
                                 guint               *n_children)
{
  const GskContainerNode *self = (const GskContainerNode *) node;

  *n_children = self->n_children;

  return self->children;
}

/*< private>
 * gsk_container_node_is_disjoint:
 * @node: a container `GskRenderNode`
 *
 * Returns `TRUE` if it is known that the child nodes are not
 * overlapping. There is no guarantee that they do overlap
 * if this function return FALSE.
 *
 * Returns: `TRUE` if children don't overlap
 */
gboolean
gsk_container_node_is_disjoint (const GskRenderNode *node)
{
  const GskContainerNode *self = (const GskContainerNode *) node;

  return self->disjoint;
}

/*** GSK_TRANSFORM_NODE ***/

/**
 * GskTransformNode:
 *
 * A render node applying a `GskTransform` to its single child node.
 */
struct _GskTransformNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GskTransform *transform;
  float dx, dy;
};

static void
gsk_transform_node_finalize (GskRenderNode *node)
{
  GskTransformNode *self = (GskTransformNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_TRANSFORM_NODE));

  gsk_render_node_unref (self->child);
  gsk_transform_unref (self->transform);

  parent_class->finalize (node);
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
      gsk_cairo_rectangle (cr, &node->bounds);
      cairo_fill (cr);
      return;
    }

  gsk_transform_to_2d (self->transform, &xx, &yx, &xy, &yy, &dx, &dy);
  cairo_matrix_init (&ctm, xx, yx, xy, yy, dx, dy);
  GSK_DEBUG (CAIRO, "CTM = { .xx = %g, .yx = %g, .xy = %g, .yy = %g, .x0 = %g, .y0 = %g }",
                    ctm.xx, ctm.yx,
                    ctm.xy, ctm.yy,
                    ctm.x0, ctm.y0);
  if (xx * yy == xy * yx)
    {
      /* broken matrix here. This can happen during transitions
       * (like when flipping an axis at the point where scale == 0)
       * and just means that nothing should be drawn.
       * But Cairo throws lots of ugly errors instead of silently
       * going on. So We silently go on.
       */
      return;
    }
  cairo_transform (cr, &ctm);

  gsk_render_node_draw (self->child, cr);
}

static gboolean
gsk_transform_node_can_diff (const GskRenderNode *node1,
                             const GskRenderNode *node2)
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
        cairo_region_translate (sub, floorf (dx), floorf (dy));
        if (floorf (dx) != dx)
          {
            cairo_region_t *tmp = cairo_region_copy (sub);
            cairo_region_translate (tmp, 1, 0);
            cairo_region_union (sub, tmp);
            cairo_region_destroy (tmp);
          }
        if (floorf (dy) != dy)
          {
            cairo_region_t *tmp = cairo_region_copy (sub);
            cairo_region_translate (tmp, 0, 1);
            cairo_region_union (sub, tmp);
            cairo_region_destroy (tmp);
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

/**
 * gsk_transform_node_new:
 * @child: The node to transform
 * @transform: (transfer none): The transform to apply
 *
 * Creates a `GskRenderNode` that will transform the given @child
 * with the given @transform.
 *
 * Returns: (transfer full) (type GskTransformNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_transform_node_new (GskRenderNode *child,
                        GskTransform  *transform)
{
  GskTransformNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (transform != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TRANSFORM_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = child->offscreen_for_opacity;

  self->child = gsk_render_node_ref (child);
  self->transform = gsk_transform_ref (transform);

  if (gsk_transform_get_category (transform) >= GSK_TRANSFORM_CATEGORY_2D_TRANSLATE)
    gsk_transform_to_translate (transform, &self->dx, &self->dy);
  else
    self->dx = self->dy = 0;

  gsk_transform_transform_bounds (self->transform,
                                  &child->bounds,
                                  &node->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (child);

  return node;
}

/**
 * gsk_transform_node_get_child:
 * @node: (type GskTransformNode): a `GskRenderNode` for a transform
 *
 * Gets the child node that is getting transformed by the given @node.
 *
 * Returns: (transfer none): The child that is getting transformed
 */
GskRenderNode *
gsk_transform_node_get_child (const GskRenderNode *node)
{
  const GskTransformNode *self = (const GskTransformNode *) node;

  return self->child;
}

/**
 * gsk_transform_node_get_transform:
 * @node: (type GskTransformNode): a `GskRenderNode` for a transform
 *
 * Retrieves the `GskTransform` used by the @node.
 *
 * Returns: (transfer none): a `GskTransform`
 */
GskTransform *
gsk_transform_node_get_transform (const GskRenderNode *node)
{
  const GskTransformNode *self = (const GskTransformNode *) node;

  return self->transform;
}

void
gsk_transform_node_get_translate (const GskRenderNode *node,
                                  float               *dx,
                                  float               *dy)
{
  const GskTransformNode *self = (const GskTransformNode *) node;

  *dx = self->dx;
  *dy = self->dy;
}

/*** GSK_OPACITY_NODE ***/

/**
 * GskOpacityNode:
 *
 * A render node controlling the opacity of its single child node.
 */
struct _GskOpacityNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  float opacity;
};

static void
gsk_opacity_node_finalize (GskRenderNode *node)
{
  GskOpacityNode *self = (GskOpacityNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_OPACITY_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
gsk_opacity_node_draw (GskRenderNode *node,
                       cairo_t       *cr)
{
  GskOpacityNode *self = (GskOpacityNode *) node;

  cairo_save (cr);

  /* clip so the push_group() creates a smaller surface */
  gsk_cairo_rectangle (cr, &node->bounds);
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

/**
 * gsk_opacity_node_new:
 * @child: The node to draw
 * @opacity: The opacity to apply
 *
 * Creates a `GskRenderNode` that will drawn the @child with reduced
 * @opacity.
 *
 * Returns: (transfer full) (type GskOpacityNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_opacity_node_new (GskRenderNode *child,
                      float          opacity)
{
  GskOpacityNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_OPACITY_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = child->offscreen_for_opacity;

  self->child = gsk_render_node_ref (child);
  self->opacity = CLAMP (opacity, 0.0, 1.0);

  graphene_rect_init_from_rect (&node->bounds, &child->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (child);

  return node;
}

/**
 * gsk_opacity_node_get_child:
 * @node: (type GskOpacityNode): a `GskRenderNode` for an opacity
 *
 * Gets the child node that is getting opacityed by the given @node.
 *
 * Returns: (transfer none): The child that is getting opacityed
 */
GskRenderNode *
gsk_opacity_node_get_child (const GskRenderNode *node)
{
  const GskOpacityNode *self = (const GskOpacityNode *) node;

  return self->child;
}

/**
 * gsk_opacity_node_get_opacity:
 * @node: (type GskOpacityNode): a `GskRenderNode` for an opacity
 *
 * Gets the transparency factor for an opacity node.
 *
 * Returns: the opacity factor
 */
float
gsk_opacity_node_get_opacity (const GskRenderNode *node)
{
  const GskOpacityNode *self = (const GskOpacityNode *) node;

  return self->opacity;
}

/*** GSK_COLOR_MATRIX_NODE ***/

/**
 * GskColorMatrixNode:
 *
 * A render node controlling the color matrix of its single child node.
 */
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
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_COLOR_MATRIX_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
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
  gsk_cairo_rectangle (cr, &node->bounds);
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

static void
gsk_color_matrix_node_diff (GskRenderNode  *node1,
                            GskRenderNode  *node2,
                            cairo_region_t *region)
{
  GskColorMatrixNode *self1 = (GskColorMatrixNode *) node1;
  GskColorMatrixNode *self2 = (GskColorMatrixNode *) node2;

  if (!graphene_vec4_equal (&self1->color_offset, &self2->color_offset))
    goto nope;

  if (!graphene_matrix_equal_fast (&self1->color_matrix, &self2->color_matrix))
    goto nope;

  gsk_render_node_diff (self1->child, self2->child, region);
  return;

nope:
  gsk_render_node_diff_impossible (node1, node2, region);
  return;
}

/**
 * gsk_color_matrix_node_new:
 * @child: The node to draw
 * @color_matrix: The matrix to apply
 * @color_offset: Values to add to the color
 *
 * Creates a `GskRenderNode` that will drawn the @child with
 * @color_matrix.
 *
 * In particular, the node will transform the operation
 *
 *     pixel = color_matrix * pixel + color_offset
 *
 * for every pixel.
 *
 * Returns: (transfer full) (type GskColorMatrixNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_color_matrix_node_new (GskRenderNode           *child,
                           const graphene_matrix_t *color_matrix,
                           const graphene_vec4_t   *color_offset)
{
  GskColorMatrixNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_COLOR_MATRIX_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = child->offscreen_for_opacity;

  self->child = gsk_render_node_ref (child);
  graphene_matrix_init_from_matrix (&self->color_matrix, color_matrix);
  graphene_vec4_init_from_vec4 (&self->color_offset, color_offset);

  graphene_rect_init_from_rect (&node->bounds, &child->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (child);

  return node;
}

/**
 * gsk_color_matrix_node_get_child:
 * @node: (type GskColorMatrixNode): a color matrix `GskRenderNode`
 *
 * Gets the child node that is getting its colors modified by the given @node.
 *
 * Returns: (transfer none): The child that is getting its colors modified
 **/
GskRenderNode *
gsk_color_matrix_node_get_child (const GskRenderNode *node)
{
  const GskColorMatrixNode *self = (const GskColorMatrixNode *) node;

  return self->child;
}

/**
 * gsk_color_matrix_node_get_color_matrix:
 * @node: (type GskColorMatrixNode): a color matrix `GskRenderNode`
 *
 * Retrieves the color matrix used by the @node.
 *
 * Returns: a 4x4 color matrix
 */
const graphene_matrix_t *
gsk_color_matrix_node_get_color_matrix (const GskRenderNode *node)
{
  const GskColorMatrixNode *self = (const GskColorMatrixNode *) node;

  return &self->color_matrix;
}

/**
 * gsk_color_matrix_node_get_color_offset:
 * @node: (type GskColorMatrixNode): a color matrix `GskRenderNode`
 *
 * Retrieves the color offset used by the @node.
 *
 * Returns: a color vector
 */
const graphene_vec4_t *
gsk_color_matrix_node_get_color_offset (const GskRenderNode *node)
{
  const GskColorMatrixNode *self = (const GskColorMatrixNode *) node;

  return &self->color_offset;
}

/*** GSK_REPEAT_NODE ***/

/**
 * GskRepeatNode:
 *
 * A render node repeating its single child node.
 */
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
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_REPEAT_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
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
  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);

  gsk_cairo_rectangle (cr, &node->bounds);
  cairo_fill (cr);
}

/**
 * gsk_repeat_node_new:
 * @bounds: The bounds of the area to be painted
 * @child: The child to repeat
 * @child_bounds: (nullable): The area of the child to repeat or %NULL to
 *     use the child's bounds
 *
 * Creates a `GskRenderNode` that will repeat the drawing of @child across
 * the given @bounds.
 *
 * Returns: (transfer full) (type GskRepeatNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_repeat_node_new (const graphene_rect_t *bounds,
                     GskRenderNode         *child,
                     const graphene_rect_t *child_bounds)
{
  GskRepeatNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_REPEAT_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = TRUE;

  graphene_rect_init_from_rect (&node->bounds, bounds);

  self->child = gsk_render_node_ref (child);

  if (child_bounds)
    graphene_rect_init_from_rect (&self->child_bounds, child_bounds);
  else
    graphene_rect_init_from_rect (&self->child_bounds, &child->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (child);

  return node;
}

/**
 * gsk_repeat_node_get_child:
 * @node: (type GskRepeatNode): a repeat `GskRenderNode`
 *
 * Retrieves the child of @node.
 *
 * Returns: (transfer none): a `GskRenderNode`
 */
GskRenderNode *
gsk_repeat_node_get_child (const GskRenderNode *node)
{
  const GskRepeatNode *self = (const GskRepeatNode *) node;

  return self->child;
}

/**
 * gsk_repeat_node_get_child_bounds:
 * @node: (type GskRepeatNode): a repeat `GskRenderNode`
 *
 * Retrieves the bounding rectangle of the child of @node.
 *
 * Returns: (transfer none): a bounding rectangle
 */
const graphene_rect_t *
gsk_repeat_node_get_child_bounds (const GskRenderNode *node)
{
  const GskRepeatNode *self = (const GskRepeatNode *) node;

  return &self->child_bounds;
}

/*** GSK_CLIP_NODE ***/

/**
 * GskClipNode:
 *
 * A render node applying a rectangular clip to its single child node.
 */
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
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CLIP_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
gsk_clip_node_draw (GskRenderNode *node,
                    cairo_t       *cr)
{
  GskClipNode *self = (GskClipNode *) node;

  cairo_save (cr);

  gsk_cairo_rectangle (cr, &self->clip);
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

/**
 * gsk_clip_node_new:
 * @child: The node to draw
 * @clip: The clip to apply
 *
 * Creates a `GskRenderNode` that will clip the @child to the area
 * given by @clip.
 *
 * Returns: (transfer full) (type GskClipNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_clip_node_new (GskRenderNode         *child,
                   const graphene_rect_t *clip)
{
  GskClipNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (clip != NULL, NULL);

  self = gsk_render_node_alloc (GSK_CLIP_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = child->offscreen_for_opacity;

  self->child = gsk_render_node_ref (child);
  graphene_rect_normalize_r (clip, &self->clip);

  graphene_rect_intersection (&self->clip, &child->bounds, &node->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (child);

  return node;
}

/**
 * gsk_clip_node_get_child:
 * @node: (type GskClipNode): a clip @GskRenderNode
 *
 * Gets the child node that is getting clipped by the given @node.
 *
 * Returns: (transfer none): The child that is getting clipped
 **/
GskRenderNode *
gsk_clip_node_get_child (const GskRenderNode *node)
{
  const GskClipNode *self = (const GskClipNode *) node;

  return self->child;
}

/**
 * gsk_clip_node_get_clip:
 * @node: (type GskClipNode): a `GskClipNode`
 *
 * Retrieves the clip rectangle for @node.
 *
 * Returns: a clip rectangle
 */
const graphene_rect_t *
gsk_clip_node_get_clip (const GskRenderNode *node)
{
  const GskClipNode *self = (const GskClipNode *) node;

  return &self->clip;
}

/*** GSK_ROUNDED_CLIP_NODE ***/

/**
 * GskRoundedClipNode:
 *
 * A render node applying a rounded rectangle clip to its single child.
 */
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
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_ROUNDED_CLIP_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
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

/**
 * gsk_rounded_clip_node_new:
 * @child: The node to draw
 * @clip: The clip to apply
 *
 * Creates a `GskRenderNode` that will clip the @child to the area
 * given by @clip.
 *
 * Returns: (transfer full) (type GskRoundedClipNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_rounded_clip_node_new (GskRenderNode         *child,
                           const GskRoundedRect  *clip)
{
  GskRoundedClipNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (clip != NULL, NULL);

  self = gsk_render_node_alloc (GSK_ROUNDED_CLIP_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = child->offscreen_for_opacity;

  self->child = gsk_render_node_ref (child);
  gsk_rounded_rect_init_copy (&self->clip, clip);

  graphene_rect_intersection (&self->clip.bounds, &child->bounds, &node->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (child);

  return node;
}

/**
 * gsk_rounded_clip_node_get_child:
 * @node: (type GskRoundedClipNode): a rounded clip `GskRenderNode`
 *
 * Gets the child node that is getting clipped by the given @node.
 *
 * Returns: (transfer none): The child that is getting clipped
 **/
GskRenderNode *
gsk_rounded_clip_node_get_child (const GskRenderNode *node)
{
  const GskRoundedClipNode *self = (const GskRoundedClipNode *) node;

  return self->child;
}

/**
 * gsk_rounded_clip_node_get_clip:
 * @node: (type GskRoundedClipNode): a rounded clip `GskRenderNode`
 *
 * Retrieves the rounded rectangle used to clip the contents of the @node.
 *
 * Returns: (transfer none): a rounded rectangle
 */
const GskRoundedRect *
gsk_rounded_clip_node_get_clip (const GskRenderNode *node)
{
  const GskRoundedClipNode *self = (const GskRoundedClipNode *) node;

  return &self->clip;
}

/*** GSK_SHADOW_NODE ***/

/**
 * GskShadowNode:
 *
 * A render node drawing one or more shadows behind its single child node.
 */
struct _GskShadowNode
{
  GskRenderNode render_node;

  GskRenderNode *child;

  gsize n_shadows;
  GskShadow *shadows;
};

static void
gsk_shadow_node_finalize (GskRenderNode *node)
{
  GskShadowNode *self = (GskShadowNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_SHADOW_NODE));

  gsk_render_node_unref (self->child);
  g_free (self->shadows);

  parent_class->finalize (node);
}

static void
gsk_shadow_node_draw (GskRenderNode *node,
                      cairo_t       *cr)
{
  GskShadowNode *self = (GskShadowNode *) node;
  cairo_pattern_t *pattern;
  gsize i;

  cairo_save (cr);
  /* clip so the push_group() creates a small surface */
  gsk_cairo_rectangle (cr, &self->child->bounds);
  cairo_clip (cr);
  cairo_push_group (cr);
  gsk_render_node_draw (self->child, cr);
  pattern = cairo_pop_group (cr);
  cairo_restore (cr);

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

      clip_radius = gsk_cairo_blur_compute_pixels (shadow1->radius / 2.0);
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
      float clip_radius = gsk_cairo_blur_compute_pixels (self->shadows[i].radius / 2.0);
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

/**
 * gsk_shadow_node_new:
 * @child: The node to draw
 * @shadows: (array length=n_shadows): The shadows to apply
 * @n_shadows: number of entries in the @shadows array
 *
 * Creates a `GskRenderNode` that will draw a @child with the given
 * @shadows below it.
 *
 * Returns: (transfer full) (type GskShadowNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_shadow_node_new (GskRenderNode   *child,
                     const GskShadow *shadows,
                     gsize            n_shadows)
{
  GskShadowNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (shadows != NULL, NULL);
  g_return_val_if_fail (n_shadows > 0, NULL);

  self = gsk_render_node_alloc (GSK_SHADOW_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = child->offscreen_for_opacity;

  self->child = gsk_render_node_ref (child);
  self->n_shadows = n_shadows;
  self->shadows = g_malloc_n (n_shadows, sizeof (GskShadow));
  memcpy (self->shadows, shadows, n_shadows * sizeof (GskShadow));

  gsk_shadow_node_get_bounds (self, &node->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (child);

  return node;
}

/**
 * gsk_shadow_node_get_child:
 * @node: (type GskShadowNode): a shadow `GskRenderNode`
 *
 * Retrieves the child `GskRenderNode` of the shadow @node.
 *
 * Returns: (transfer none): the child render node
 */
GskRenderNode *
gsk_shadow_node_get_child (const GskRenderNode *node)
{
  const GskShadowNode *self = (const GskShadowNode *) node;

  return self->child;
}

/**
 * gsk_shadow_node_get_shadow:
 * @node: (type GskShadowNode): a shadow `GskRenderNode`
 * @i: the given index
 *
 * Retrieves the shadow data at the given index @i.
 *
 * Returns: (transfer none): the shadow data
 */
const GskShadow *
gsk_shadow_node_get_shadow (const GskRenderNode *node,
                            gsize                i)
{
  const GskShadowNode *self = (const GskShadowNode *) node;

  return &self->shadows[i];
}

/**
 * gsk_shadow_node_get_n_shadows:
 * @node: (type GskShadowNode): a shadow `GskRenderNode`
 *
 * Retrieves the number of shadows in the @node.
 *
 * Returns: the number of shadows.
 */
gsize
gsk_shadow_node_get_n_shadows (const GskRenderNode *node)
{
  const GskShadowNode *self = (const GskShadowNode *) node;

  return self->n_shadows;
}

/*** GSK_BLEND_NODE ***/

/**
 * GskBlendNode:
 *
 * A render node applying a blending function between its two child nodes.
 */
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
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_BLEND_NODE));

  gsk_render_node_unref (self->bottom);
  gsk_render_node_unref (self->top);

  parent_class->finalize (node);
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

/**
 * gsk_blend_node_new:
 * @bottom: The bottom node to be drawn
 * @top: The node to be blended onto the @bottom node
 * @blend_mode: The blend mode to use
 *
 * Creates a `GskRenderNode` that will use @blend_mode to blend the @top
 * node onto the @bottom node.
 *
 * Returns: (transfer full) (type GskBlendNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_blend_node_new (GskRenderNode *bottom,
                    GskRenderNode *top,
                    GskBlendMode   blend_mode)
{
  GskBlendNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (bottom), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (top), NULL);

  self = gsk_render_node_alloc (GSK_BLEND_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = TRUE;

  self->bottom = gsk_render_node_ref (bottom);
  self->top = gsk_render_node_ref (top);
  self->blend_mode = blend_mode;

  graphene_rect_union (&bottom->bounds, &top->bounds, &node->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (bottom) || gsk_render_node_prefers_high_depth (top);

  return node;
}

/**
 * gsk_blend_node_get_bottom_child:
 * @node: (type GskBlendNode): a blending `GskRenderNode`
 *
 * Retrieves the bottom `GskRenderNode` child of the @node.
 *
 * Returns: (transfer none): the bottom child node
 */
GskRenderNode *
gsk_blend_node_get_bottom_child (const GskRenderNode *node)
{
  const GskBlendNode *self = (const GskBlendNode *) node;

  return self->bottom;
}

/**
 * gsk_blend_node_get_top_child:
 * @node: (type GskBlendNode): a blending `GskRenderNode`
 *
 * Retrieves the top `GskRenderNode` child of the @node.
 *
 * Returns: (transfer none): the top child node
 */
GskRenderNode *
gsk_blend_node_get_top_child (const GskRenderNode *node)
{
  const GskBlendNode *self = (const GskBlendNode *) node;

  return self->top;
}

/**
 * gsk_blend_node_get_blend_mode:
 * @node: (type GskBlendNode): a blending `GskRenderNode`
 *
 * Retrieves the blend mode used by @node.
 *
 * Returns: the blend mode
 */
GskBlendMode
gsk_blend_node_get_blend_mode (const GskRenderNode *node)
{
  const GskBlendNode *self = (const GskBlendNode *) node;

  return self->blend_mode;
}

/*** GSK_CROSS_FADE_NODE ***/

/**
 * GskCrossFadeNode:
 *
 * A render node cross fading between two child nodes.
 */
struct _GskCrossFadeNode
{
  GskRenderNode render_node;

  GskRenderNode *start;
  GskRenderNode *end;
  float          progress;
};

static void
gsk_cross_fade_node_finalize (GskRenderNode *node)
{
  GskCrossFadeNode *self = (GskCrossFadeNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CROSS_FADE_NODE));

  gsk_render_node_unref (self->start);
  gsk_render_node_unref (self->end);

  parent_class->finalize (node);
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

/**
 * gsk_cross_fade_node_new:
 * @start: The start node to be drawn
 * @end: The node to be cross_fadeed onto the @start node
 * @progress: How far the fade has progressed from start to end. The value will
 *     be clamped to the range [0 ... 1]
 *
 * Creates a `GskRenderNode` that will do a cross-fade between @start and @end.
 *
 * Returns: (transfer full) (type GskCrossFadeNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_cross_fade_node_new (GskRenderNode *start,
                         GskRenderNode *end,
                         float          progress)
{
  GskCrossFadeNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (start), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (end), NULL);

  self = gsk_render_node_alloc (GSK_CROSS_FADE_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = TRUE;

  self->start = gsk_render_node_ref (start);
  self->end = gsk_render_node_ref (end);
  self->progress = CLAMP (progress, 0.0, 1.0);

  graphene_rect_union (&start->bounds, &end->bounds, &node->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (start) || gsk_render_node_prefers_high_depth (end);

  return node;
}

/**
 * gsk_cross_fade_node_get_start_child:
 * @node: (type GskCrossFadeNode): a cross-fading `GskRenderNode`
 *
 * Retrieves the child `GskRenderNode` at the beginning of the cross-fade.
 *
 * Returns: (transfer none): a `GskRenderNode`
 */
GskRenderNode *
gsk_cross_fade_node_get_start_child (const GskRenderNode *node)
{
  const GskCrossFadeNode *self = (const GskCrossFadeNode *) node;

  return self->start;
}

/**
 * gsk_cross_fade_node_get_end_child:
 * @node: (type GskCrossFadeNode): a cross-fading `GskRenderNode`
 *
 * Retrieves the child `GskRenderNode` at the end of the cross-fade.
 *
 * Returns: (transfer none): a `GskRenderNode`
 */
GskRenderNode *
gsk_cross_fade_node_get_end_child (const GskRenderNode *node)
{
  const GskCrossFadeNode *self = (const GskCrossFadeNode *) node;

  return self->end;
}

/**
 * gsk_cross_fade_node_get_progress:
 * @node: (type GskCrossFadeNode): a cross-fading `GskRenderNode`
 *
 * Retrieves the progress value of the cross fade.
 *
 * Returns: the progress value, between 0 and 1
 */
float
gsk_cross_fade_node_get_progress (const GskRenderNode *node)
{
  const GskCrossFadeNode *self = (const GskCrossFadeNode *) node;

  return self->progress;
}

/*** GSK_TEXT_NODE ***/

/**
 * GskTextNode:
 *
 * A render node drawing a set of glyphs.
 */
struct _GskTextNode
{
  GskRenderNode render_node;

  PangoFont *font;
  gboolean has_color_glyphs;

  GdkRGBA color;
  graphene_point_t offset;

  guint num_glyphs;
  PangoGlyphInfo *glyphs;
};

static void
gsk_text_node_finalize (GskRenderNode *node)
{
  GskTextNode *self = (GskTextNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_TEXT_NODE));

  g_object_unref (self->font);
  g_free (self->glyphs);

  parent_class->finalize (node);
}

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
              info1->attr.is_cluster_start == info2->attr.is_cluster_start &&
              info1->attr.is_color == info2->attr.is_color)
            continue;

          gsk_render_node_diff_impossible (node1, node2, region);
          return;
        }

      return;
    }

  gsk_render_node_diff_impossible (node1, node2, region);
}

/**
 * gsk_text_node_new:
 * @font: the `PangoFont` containing the glyphs
 * @glyphs: the `PangoGlyphString` to render
 * @color: the foreground color to render with
 * @offset: offset of the baseline
 *
 * Creates a render node that renders the given glyphs.
 *
 * Note that @color may not be used if the font contains
 * color glyphs.
 *
 * Returns: (nullable) (transfer full) (type GskTextNode): a new `GskRenderNode`
 */
GskRenderNode *
gsk_text_node_new (PangoFont              *font,
                   PangoGlyphString       *glyphs,
                   const GdkRGBA          *color,
                   const graphene_point_t *offset)
{
  GskTextNode *self;
  GskRenderNode *node;
  PangoRectangle ink_rect;
  PangoGlyphInfo *glyph_infos;
  int n;

  pango_glyph_string_extents (glyphs, font, &ink_rect, NULL);
  pango_extents_to_pixels (&ink_rect, NULL);

  /* Don't create nodes with empty bounds */
  if (ink_rect.width == 0 || ink_rect.height == 0)
    return NULL;

  self = gsk_render_node_alloc (GSK_TEXT_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  self->font = g_object_ref (font);
  self->color = *color;
  self->offset = *offset;
  self->has_color_glyphs = FALSE;

  glyph_infos = g_malloc_n (glyphs->num_glyphs, sizeof (PangoGlyphInfo));

  n = 0;
  for (int i = 0; i < glyphs->num_glyphs; i++)
    {
      /* skip empty glyphs */
      if (glyphs->glyphs[i].glyph == PANGO_GLYPH_EMPTY)
        continue;

      glyph_infos[n] = glyphs->glyphs[i];

      if (glyphs->glyphs[i].attr.is_color)
        self->has_color_glyphs = TRUE;

      n++;
    }

  self->glyphs = glyph_infos;
  self->num_glyphs = n;

  graphene_rect_init (&node->bounds,
                      offset->x + ink_rect.x - 1,
                      offset->y + ink_rect.y - 1,
                      ink_rect.width + 2,
                      ink_rect.height + 2);

  return node;
}

/**
 * gsk_text_node_get_color:
 * @node: (type GskTextNode): a text `GskRenderNode`
 *
 * Retrieves the color used by the text @node.
 *
 * Returns: (transfer none): the text color
 */
const GdkRGBA *
gsk_text_node_get_color (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return &self->color;
}

/**
 * gsk_text_node_get_font:
 * @node: (type GskTextNode): The `GskRenderNode`
 *
 * Returns the font used by the text @node.
 *
 * Returns: (transfer none): the font
 */
PangoFont *
gsk_text_node_get_font (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return self->font;
}

/**
 * gsk_text_node_has_color_glyphs:
 * @node: (type GskTextNode): a text `GskRenderNode`
 *
 * Checks whether the text @node has color glyphs.
 *
 * Returns: %TRUE if the text node has color glyphs
 *
 * Since: 4.2
 */
gboolean
gsk_text_node_has_color_glyphs (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return self->has_color_glyphs;
}

/**
 * gsk_text_node_get_num_glyphs:
 * @node: (type GskTextNode): a text `GskRenderNode`
 *
 * Retrieves the number of glyphs in the text node.
 *
 * Returns: the number of glyphs
 */
guint
gsk_text_node_get_num_glyphs (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return self->num_glyphs;
}

/**
 * gsk_text_node_get_glyphs:
 * @node: (type GskTextNode): a text `GskRenderNode`
 * @n_glyphs: (out) (optional): the number of glyphs returned
 *
 * Retrieves the glyph information in the @node.
 *
 * Returns: (transfer none) (array length=n_glyphs): the glyph information
 */
const PangoGlyphInfo *
gsk_text_node_get_glyphs (const GskRenderNode *node,
                          guint               *n_glyphs)
{
  const GskTextNode *self = (const GskTextNode *) node;

  if (n_glyphs != NULL)
    *n_glyphs = self->num_glyphs;

  return self->glyphs;
}

/**
 * gsk_text_node_get_offset:
 * @node: (type GskTextNode): a text `GskRenderNode`
 *
 * Retrieves the offset applied to the text.
 *
 * Returns: (transfer none): a point with the horizontal and vertical offsets
 */
const graphene_point_t *
gsk_text_node_get_offset (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return &self->offset;
}

/*** GSK_GLYPH_NODE ***/

struct _GskGlyphNode
{
  GskRenderNode render_node;

  hb_font_t *font;
  hb_codepoint_t glyph;
  unsigned int palette_index;
  GdkRGBA foreground_color;
  unsigned int n_colors;
  GdkRGBA *colors;
};

static void
gsk_glyph_node_finalize (GskRenderNode *node)
{
  GskGlyphNode *self = (GskGlyphNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_GLYPH_NODE));

  hb_font_destroy (self->font);
  g_free (self->colors);

  parent_class->finalize (node);
}

static void
gsk_glyph_node_draw (GskRenderNode *node,
                     cairo_t       *cr)
{
  GskGlyphNode *self = (GskGlyphNode *) node;
  unsigned int upem;
  cairo_font_face_t *cairo_face;
  cairo_matrix_t font_matrix, ctm;
  cairo_font_options_t *font_options;
  cairo_scaled_font_t *scaled_font;
  hb_glyph_extents_t extents;
  double scale;
  cairo_glyph_t glyph;

  if (!hb_font_get_glyph_extents (self->font, self->glyph, &extents))
    return;

  if (extents.width <= 0 || extents.height == 0)
    return;

  cairo_save (cr);

  upem = hb_face_get_upem (hb_font_get_face (self->font));

  cairo_face = hb_cairo_font_face_create_for_font (self->font);
  hb_cairo_font_face_set_scale_factor (cairo_face, 1 << 6);

  cairo_matrix_init_identity (&ctm);
  cairo_matrix_init_scale (&font_matrix, (double)upem, (double)upem);

  font_options = cairo_font_options_create ();
  cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_NONE);
  cairo_font_options_set_hint_metrics (font_options, CAIRO_HINT_METRICS_OFF);
#ifdef CAIRO_COLOR_PALETTE_DEFAULT
  cairo_font_options_set_color_palette (font_options, self->palette_index);
#endif
#ifdef HAVE_CAIRO_FONT_OPTIONS_SET_CUSTOM_PALETTE_COLOR
  for (int i = 0; i < self->n_colors; i++)
    cairo_font_options_set_custom_palette_color (font_options, i,
                                                 self->colors[i].red,
                                                 self->colors[i].green,
                                                 self->colors[i].blue,
                                                 self->colors[i].alpha);
#endif

  scaled_font = cairo_scaled_font_create (cairo_face, &font_matrix, &ctm, font_options);

  cairo_font_options_destroy (font_options);
  cairo_font_face_destroy (cairo_face);

  cairo_set_scaled_font (cr, scaled_font);

  cairo_scaled_font_destroy (scaled_font);

  scale = node->bounds.size.width * (1 << 6) / (double) extents.width;

  cairo_scale (cr, scale, scale);

  gdk_cairo_set_source_rgba (cr, &self->foreground_color);

  glyph.index = self->glyph;
  glyph.x = - extents.x_bearing / (1 << 6);
  glyph.y = extents.y_bearing / (1 << 6);

  cairo_show_glyphs (cr, &glyph, 1);

  cairo_restore (cr);
}

static void
gsk_glyph_node_diff (GskRenderNode  *node1,
                     GskRenderNode  *node2,
                     cairo_region_t *region)
{
  GskGlyphNode *self1 = (GskGlyphNode *) node1;
  GskGlyphNode *self2 = (GskGlyphNode *) node2;

  if (self1->font == self2->font &&
      self1->glyph == self2->glyph &&
      self1->palette_index == self2->palette_index &&
      gdk_rgba_equal (&self1->foreground_color, &self2->foreground_color) &&
      self1->n_colors == self2->n_colors)
    {
      for (unsigned int i = 0; i < self1->n_colors; i++)
        {
          if (!gdk_rgba_equal (&self1->colors[i], &self2->colors[i]))
            {
              gsk_render_node_diff_impossible (node1, node2, region);
              return;
            }
        }
      return;
    }

  gsk_render_node_diff_impossible (node1, node2, region);
}

GskRenderNode *
gsk_glyph_node_new (const graphene_rect_t *bounds,
                    hb_font_t             *font,
                    hb_codepoint_t         glyph,
                    unsigned int           palette_index,
                    const GdkRGBA         *foreground_color,
                    unsigned int           n_colors,
                    const GdkRGBA         *colors)
{
  GskGlyphNode *self;
  GskRenderNode *node;

  self = gsk_render_node_alloc (GSK_GLYPH_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = FALSE;

  self->font = hb_font_reference (font);
  self->glyph = glyph;
  self->palette_index = palette_index;
  self->foreground_color = *foreground_color;
  self->n_colors = n_colors;

  self->colors = g_new (GdkRGBA, n_colors);
  for (unsigned int i = 0; i < n_colors; i++)
    self->colors[i] = colors[i];

  graphene_rect_init_from_rect (&node->bounds, bounds);

  return node;
}

hb_font_t *
gsk_glyph_node_get_font (const GskRenderNode *node)
{
  GskGlyphNode *self = (GskGlyphNode *) node;

  return self->font;
}

hb_codepoint_t
gsk_glyph_node_get_glyph (const GskRenderNode *node)
{
  GskGlyphNode *self = (GskGlyphNode *) node;

  return self->glyph;
}

unsigned int
gsk_glyph_node_get_palette_index (const GskRenderNode *node)
{
  GskGlyphNode *self = (GskGlyphNode *) node;

  return self->palette_index;
}

const GdkRGBA *
gsk_glyph_node_get_foreground_color (const GskRenderNode *node)
{
  GskGlyphNode *self = (GskGlyphNode *) node;

  return &self->foreground_color;
}

unsigned int
gsk_glyph_node_get_n_colors (const GskRenderNode *node)
{
  GskGlyphNode *self = (GskGlyphNode *) node;

  return self->n_colors;
}

const GdkRGBA *
gsk_glyph_node_get_colors (const GskRenderNode *node)
{
  GskGlyphNode *self = (GskGlyphNode *) node;

  return self->colors;
}

/*** GSK_BLUR_NODE ***/

/**
 * GskBlurNode:
 *
 * A render node applying a blur effect to its single child.
 */
struct _GskBlurNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  float radius;
};

static void
gsk_blur_node_finalize (GskRenderNode *node)
{
  GskBlurNode *self = (GskBlurNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_BLUR_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
blur_once (cairo_surface_t *src,
           cairo_surface_t *dest,
           int radius,
           guchar *div_kernel_size)
{
  int width, height, src_rowstride, dest_rowstride, n_channels;
  guchar *p_src, *p_dest, *c1, *c2;
  int x, y, i, i1, i2, width_minus_1, height_minus_1, radius_plus_1;
  int r, g, b, a;
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

  g_assert (radius >= 0);

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
  gsk_cairo_rectangle (cr, &node->bounds);
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
  cairo_rectangle (cr,
                   node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_fill (cr);

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

      clip_radius = ceil (gsk_cairo_blur_compute_pixels (self1->radius / 2.0));
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

/**
 * gsk_blur_node_new:
 * @child: the child node to blur
 * @radius: the blur radius. Must be positive
 *
 * Creates a render node that blurs the child.
 *
 * Returns: (transfer full) (type GskBlurNode): a new `GskRenderNode`
 */
GskRenderNode *
gsk_blur_node_new (GskRenderNode *child,
                   float          radius)
{
  GskBlurNode *self;
  GskRenderNode *node;
  float clip_radius;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (radius >= 0, NULL);

  self = gsk_render_node_alloc (GSK_BLUR_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = child->offscreen_for_opacity;

  self->child = gsk_render_node_ref (child);
  self->radius = radius;

  clip_radius = gsk_cairo_blur_compute_pixels (radius / 2.0);

  graphene_rect_init_from_rect (&node->bounds, &child->bounds);
  graphene_rect_inset (&self->render_node.bounds,
                       - clip_radius,
                       - clip_radius);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (child);

  return node;
}

/**
 * gsk_blur_node_get_child:
 * @node: (type GskBlurNode): a blur `GskRenderNode`
 *
 * Retrieves the child `GskRenderNode` of the blur @node.
 *
 * Returns: (transfer none): the blurred child node
 */
GskRenderNode *
gsk_blur_node_get_child (const GskRenderNode *node)
{
  const GskBlurNode *self = (const GskBlurNode *) node;

  return self->child;
}

/**
 * gsk_blur_node_get_radius:
 * @node: (type GskBlurNode): a blur `GskRenderNode`
 *
 * Retrieves the blur radius of the @node.
 *
 * Returns: the blur radius
 */
float
gsk_blur_node_get_radius (const GskRenderNode *node)
{
  const GskBlurNode *self = (const GskBlurNode *) node;

  return self->radius;
}

/*** GSK_DEBUG_NODE ***/

/**
 * GskDebugNode:
 *
 * A render node that emits a debugging message when drawing its
 * child node.
 */
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
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_DEBUG_NODE));

  gsk_render_node_unref (self->child);
  g_free (self->message);

  parent_class->finalize (node);
}

static void
gsk_debug_node_draw (GskRenderNode *node,
                      cairo_t       *cr)
{
  GskDebugNode *self = (GskDebugNode *) node;

  gsk_render_node_draw (self->child, cr);
}

static gboolean
gsk_debug_node_can_diff (const GskRenderNode *node1,
                         const GskRenderNode *node2)
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

/**
 * gsk_debug_node_new:
 * @child: The child to add debug info for
 * @message: (transfer full): The debug message
 *
 * Creates a `GskRenderNode` that will add debug information about
 * the given @child.
 *
 * Adding this node has no visual effect.
 *
 * Returns: (transfer full) (type GskDebugNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_debug_node_new (GskRenderNode *child,
                    char          *message)
{
  GskDebugNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_DEBUG_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = child->offscreen_for_opacity;

  self->child = gsk_render_node_ref (child);
  self->message = message;

  graphene_rect_init_from_rect (&node->bounds, &child->bounds);

  node->prefers_high_depth = gsk_render_node_prefers_high_depth (child);

  return node;
}

/**
 * gsk_debug_node_get_child:
 * @node: (type GskDebugNode): a debug `GskRenderNode`
 *
 * Gets the child node that is getting drawn by the given @node.
 *
 * Returns: (transfer none): the child `GskRenderNode`
 **/
GskRenderNode *
gsk_debug_node_get_child (const GskRenderNode *node)
{
  const GskDebugNode *self = (const GskDebugNode *) node;

  return self->child;
}

/**
 * gsk_debug_node_get_message:
 * @node: (type GskDebugNode): a debug `GskRenderNode`
 *
 * Gets the debug message that was set on this node
 *
 * Returns: (transfer none): The debug message
 **/
const char *
gsk_debug_node_get_message (const GskRenderNode *node)
{
  const GskDebugNode *self = (const GskDebugNode *) node;

  return self->message;
}

/*** GSK_GL_SHADER_NODE ***/

/**
 * GskGLShaderNode:
 *
 * A render node using a GL shader when drawing its children nodes.
 */
struct _GskGLShaderNode
{
  GskRenderNode render_node;

  GskGLShader *shader;
  GBytes *args;
  GskRenderNode **children;
  guint n_children;
};

static void
gsk_gl_shader_node_finalize (GskRenderNode *node)
{
  GskGLShaderNode *self = (GskGLShaderNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_GL_SHADER_NODE));

  for (guint i = 0; i < self->n_children; i++)
    gsk_render_node_unref (self->children[i]);
  g_free (self->children);

  g_bytes_unref (self->args);

  g_object_unref (self->shader);

  parent_class->finalize (node);
}

static void
gsk_gl_shader_node_draw (GskRenderNode *node,
                         cairo_t       *cr)
{
  cairo_set_source_rgb (cr, 255 / 255., 105 / 255., 180 / 255.);
  gsk_cairo_rectangle (cr, &node->bounds);
  cairo_fill (cr);
}

static void
gsk_gl_shader_node_diff (GskRenderNode  *node1,
                         GskRenderNode  *node2,
                         cairo_region_t *region)
{
  GskGLShaderNode *self1 = (GskGLShaderNode *) node1;
  GskGLShaderNode *self2 = (GskGLShaderNode *) node2;

  if (graphene_rect_equal (&node1->bounds, &node2->bounds) &&
      self1->shader == self2->shader &&
      g_bytes_compare (self1->args, self2->args) == 0 &&
      self1->n_children == self2->n_children)
    {
      cairo_region_t *child_region = cairo_region_create();
      for (guint i = 0; i < self1->n_children; i++)
        gsk_render_node_diff (self1->children[i], self2->children[i], child_region);
      if (!cairo_region_is_empty (child_region))
        gsk_render_node_diff_impossible (node1, node2, region);
      cairo_region_destroy (child_region);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, region);
    }
}

/**
 * gsk_gl_shader_node_new:
 * @shader: the `GskGLShader`
 * @bounds: the rectangle to render the shader into
 * @args: Arguments for the uniforms
 * @children: (nullable) (array length=n_children): array of child nodes,
 *   these will be rendered to textures and used as input.
 * @n_children: Length of @children (currently the GL backend supports
 *   up to 4 children)
 *
 * Creates a `GskRenderNode` that will render the given @shader into the
 * area given by @bounds.
 *
 * The @args is a block of data to use for uniform input, as per types and
 * offsets defined by the @shader. Normally this is generated by
 * [method@Gsk.GLShader.format_args] or [struct@Gsk.ShaderArgsBuilder].
 *
 * See [class@Gsk.GLShader] for details about how the shader should be written.
 *
 * All the children will be rendered into textures (if they aren't already
 * `GskTextureNodes`, which will be used directly). These textures will be
 * sent as input to the shader.
 *
 * If the renderer doesn't support GL shaders, or if there is any problem
 * when compiling the shader, then the node will draw pink. You should use
 * [method@Gsk.GLShader.compile] to ensure the @shader will work for the
 * renderer before using it.
 *
 * Returns: (transfer full) (type GskGLShaderNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_gl_shader_node_new (GskGLShader           *shader,
                        const graphene_rect_t *bounds,
                        GBytes                *args,
                        GskRenderNode        **children,
                        guint                  n_children)
{
  GskGLShaderNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), NULL);
  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (args != NULL, NULL);
  g_return_val_if_fail (g_bytes_get_size (args) == gsk_gl_shader_get_args_size (shader), NULL);
  g_return_val_if_fail ((children == NULL && n_children == 0) ||
                        (n_children == gsk_gl_shader_get_n_textures (shader)), NULL);

  self = gsk_render_node_alloc (GSK_GL_SHADER_NODE);
  node = (GskRenderNode *) self;
  node->offscreen_for_opacity = TRUE;

  graphene_rect_init_from_rect (&node->bounds, bounds);
  self->shader = g_object_ref (shader);

  self->args = g_bytes_ref (args);

  self->n_children = n_children;
  if (n_children > 0)
    {
      self->children = g_malloc_n (n_children, sizeof (GskRenderNode *));
      for (guint i = 0; i < n_children; i++)
        {
          self->children[i] = gsk_render_node_ref (children[i]);
          node->prefers_high_depth |= gsk_render_node_prefers_high_depth (children[i]);
        }
    }

  return node;
}

/**
 * gsk_gl_shader_node_get_n_children:
 * @node: (type GskGLShaderNode): a `GskRenderNode` for a gl shader
 *
 * Returns the number of children
 *
 * Returns: The number of children
 */
guint
gsk_gl_shader_node_get_n_children (const GskRenderNode *node)
{
  const GskGLShaderNode *self = (const GskGLShaderNode *) node;

  return self->n_children;
}

/**
 * gsk_gl_shader_node_get_child:
 * @node: (type GskGLShaderNode): a `GskRenderNode` for a gl shader
 * @idx: the position of the child to get
 *
 * Gets one of the children.
 *
 * Returns: (transfer none): the @idx'th child of @node
 */
GskRenderNode *
gsk_gl_shader_node_get_child (const GskRenderNode *node,
                              guint                idx)
{
  const GskGLShaderNode *self = (const GskGLShaderNode *) node;

  return self->children[idx];
}

/**
 * gsk_gl_shader_node_get_shader:
 * @node: (type GskGLShaderNode): a `GskRenderNode` for a gl shader
 *
 * Gets shader code for the node.
 *
 * Returns: (transfer none): the `GskGLShader` shader
 */
GskGLShader *
gsk_gl_shader_node_get_shader (const GskRenderNode *node)
{
  const GskGLShaderNode *self = (const GskGLShaderNode *) node;

  return self->shader;
}

/**
 * gsk_gl_shader_node_get_args:
 * @node: (type GskGLShaderNode): a `GskRenderNode` for a gl shader
 *
 * Gets args for the node.
 *
 * Returns: (transfer none): A `GBytes` with the uniform arguments
 */
GBytes *
gsk_gl_shader_node_get_args (const GskRenderNode *node)
{
  const GskGLShaderNode *self = (const GskGLShaderNode *) node;

  return self->args;
}

GType gsk_render_node_types[GSK_RENDER_NODE_TYPE_N_TYPES];

#ifndef I_
# define I_(str) g_intern_static_string ((str))
#endif

#define GSK_DEFINE_RENDER_NODE_TYPE(type_name, TYPE_ENUM_VALUE) \
GType \
type_name ## _get_type (void) { \
  gsk_render_node_init_types (); \
  g_assert (gsk_render_node_types[TYPE_ENUM_VALUE] != G_TYPE_INVALID); \
  return gsk_render_node_types[TYPE_ENUM_VALUE]; \
}

GSK_DEFINE_RENDER_NODE_TYPE (gsk_container_node, GSK_CONTAINER_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_cairo_node, GSK_CAIRO_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_color_node, GSK_COLOR_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_linear_gradient_node, GSK_LINEAR_GRADIENT_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_repeating_linear_gradient_node, GSK_REPEATING_LINEAR_GRADIENT_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_radial_gradient_node, GSK_RADIAL_GRADIENT_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_repeating_radial_gradient_node, GSK_REPEATING_RADIAL_GRADIENT_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_conic_gradient_node, GSK_CONIC_GRADIENT_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_border_node, GSK_BORDER_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_texture_node, GSK_TEXTURE_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_inset_shadow_node, GSK_INSET_SHADOW_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_outset_shadow_node, GSK_OUTSET_SHADOW_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_transform_node, GSK_TRANSFORM_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_opacity_node, GSK_OPACITY_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_color_matrix_node, GSK_COLOR_MATRIX_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_repeat_node, GSK_REPEAT_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_clip_node, GSK_CLIP_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_rounded_clip_node, GSK_ROUNDED_CLIP_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_shadow_node, GSK_SHADOW_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_blend_node, GSK_BLEND_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_cross_fade_node, GSK_CROSS_FADE_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_text_node, GSK_TEXT_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_glyph_node, GSK_GLYPH_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_blur_node, GSK_BLUR_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_gl_shader_node, GSK_GL_SHADER_NODE)
GSK_DEFINE_RENDER_NODE_TYPE (gsk_debug_node, GSK_DEBUG_NODE)

static void
gsk_render_node_init_types_once (void)
{
  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_CONTAINER_NODE,
      sizeof (GskContainerNode),
      NULL,
      gsk_container_node_finalize,
      gsk_container_node_draw,
      NULL,
      gsk_container_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskContainerNode"), &node_info);
    gsk_render_node_types[GSK_CONTAINER_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_CAIRO_NODE,
      sizeof (GskCairoNode),
      NULL,
      gsk_cairo_node_finalize,
      gsk_cairo_node_draw,
      NULL,
      NULL,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskCairoNode"), &node_info);
    gsk_render_node_types[GSK_CAIRO_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_COLOR_NODE,
      sizeof (GskColorNode),
      NULL,
      NULL,
      gsk_color_node_draw,
      NULL,
      gsk_color_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskColorNode"), &node_info);
    gsk_render_node_types[GSK_COLOR_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_LINEAR_GRADIENT_NODE,
      sizeof (GskLinearGradientNode),
      NULL,
      gsk_linear_gradient_node_finalize,
      gsk_linear_gradient_node_draw,
      NULL,
      gsk_linear_gradient_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskLinearGradientNode"), &node_info);
    gsk_render_node_types[GSK_LINEAR_GRADIENT_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_REPEATING_LINEAR_GRADIENT_NODE,
      sizeof (GskLinearGradientNode),
      NULL,
      gsk_linear_gradient_node_finalize,
      gsk_linear_gradient_node_draw,
      NULL,
      gsk_linear_gradient_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskRepeatingLinearGradientNode"), &node_info);
    gsk_render_node_types[GSK_REPEATING_LINEAR_GRADIENT_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_RADIAL_GRADIENT_NODE,
      sizeof (GskRadialGradientNode),
      NULL,
      gsk_radial_gradient_node_finalize,
      gsk_radial_gradient_node_draw,
      NULL,
      gsk_radial_gradient_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskRadialGradientNode"), &node_info);
    gsk_render_node_types[GSK_RADIAL_GRADIENT_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_REPEATING_RADIAL_GRADIENT_NODE,
      sizeof (GskRadialGradientNode),
      NULL,
      gsk_radial_gradient_node_finalize,
      gsk_radial_gradient_node_draw,
      NULL,
      gsk_radial_gradient_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskRepeatingRadialGradientNode"), &node_info);
    gsk_render_node_types[GSK_REPEATING_RADIAL_GRADIENT_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_CONIC_GRADIENT_NODE,
      sizeof (GskConicGradientNode),
      NULL,
      gsk_conic_gradient_node_finalize,
      gsk_conic_gradient_node_draw,
      NULL,
      gsk_conic_gradient_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskConicGradientNode"), &node_info);
    gsk_render_node_types[GSK_CONIC_GRADIENT_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_BORDER_NODE,
      sizeof (GskBorderNode),
      NULL,
      NULL,
      gsk_border_node_draw,
      NULL,
      gsk_border_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskBorderNode"), &node_info);
    gsk_render_node_types[GSK_BORDER_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_TEXTURE_NODE,
      sizeof (GskTextureNode),
      NULL,
      gsk_texture_node_finalize,
      gsk_texture_node_draw,
      NULL,
      gsk_texture_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskTextureNode"), &node_info);
    gsk_render_node_types[GSK_TEXTURE_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_INSET_SHADOW_NODE,
      sizeof (GskInsetShadowNode),
      NULL,
      NULL,
      gsk_inset_shadow_node_draw,
      NULL,
      gsk_inset_shadow_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskInsetShadowNode"), &node_info);
    gsk_render_node_types[GSK_INSET_SHADOW_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_OUTSET_SHADOW_NODE,
      sizeof (GskOutsetShadowNode),
      NULL,
      NULL,
      gsk_outset_shadow_node_draw,
      NULL,
      gsk_outset_shadow_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskOutsetShadowNode"), &node_info);
    gsk_render_node_types[GSK_OUTSET_SHADOW_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_TRANSFORM_NODE,
      sizeof (GskTransformNode),
      NULL,
      gsk_transform_node_finalize,
      gsk_transform_node_draw,
      gsk_transform_node_can_diff,
      gsk_transform_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskTransformNode"), &node_info);
    gsk_render_node_types[GSK_TRANSFORM_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_OPACITY_NODE,
      sizeof (GskOpacityNode),
      NULL,
      gsk_opacity_node_finalize,
      gsk_opacity_node_draw,
      NULL,
      gsk_opacity_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskOpacityNode"), &node_info);
    gsk_render_node_types[GSK_OPACITY_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_COLOR_MATRIX_NODE,
      sizeof (GskColorMatrixNode),
      NULL,
      gsk_color_matrix_node_finalize,
      gsk_color_matrix_node_draw,
      NULL,
      gsk_color_matrix_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskColorMatrixNode"), &node_info);
    gsk_render_node_types[GSK_COLOR_MATRIX_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_REPEAT_NODE,
      sizeof (GskRepeatNode),
      NULL,
      gsk_repeat_node_finalize,
      gsk_repeat_node_draw,
      NULL,
      NULL,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskRepeatNode"), &node_info);
    gsk_render_node_types[GSK_REPEAT_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_CLIP_NODE,
      sizeof (GskClipNode),
      NULL,
      gsk_clip_node_finalize,
      gsk_clip_node_draw,
      NULL,
      gsk_clip_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskClipNode"), &node_info);
    gsk_render_node_types[GSK_CLIP_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_ROUNDED_CLIP_NODE,
      sizeof (GskRoundedClipNode),
      NULL,
      gsk_rounded_clip_node_finalize,
      gsk_rounded_clip_node_draw,
      NULL,
      gsk_rounded_clip_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskRoundedClipNode"), &node_info);
    gsk_render_node_types[GSK_ROUNDED_CLIP_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_SHADOW_NODE,
      sizeof (GskShadowNode),
      NULL,
      gsk_shadow_node_finalize,
      gsk_shadow_node_draw,
      NULL,
      gsk_shadow_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskShadowNode"), &node_info);
    gsk_render_node_types[GSK_SHADOW_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_BLEND_NODE,
      sizeof (GskBlendNode),
      NULL,
      gsk_blend_node_finalize,
      gsk_blend_node_draw,
      NULL,
      gsk_blend_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskBlendNode"), &node_info);
    gsk_render_node_types[GSK_BLEND_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_CROSS_FADE_NODE,
      sizeof (GskCrossFadeNode),
      NULL,
      gsk_cross_fade_node_finalize,
      gsk_cross_fade_node_draw,
      NULL,
      gsk_cross_fade_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskCrossFadeNode"), &node_info);
    gsk_render_node_types[GSK_CROSS_FADE_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_TEXT_NODE,
      sizeof (GskTextNode),
      NULL,
      gsk_text_node_finalize,
      gsk_text_node_draw,
      NULL,
      gsk_text_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskTextNode"), &node_info);
    gsk_render_node_types[GSK_TEXT_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_GLYPH_NODE,
      sizeof (GskGlyphNode),
      NULL,
      gsk_glyph_node_finalize,
      gsk_glyph_node_draw,
      NULL,
      gsk_glyph_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskGlyphNode"), &node_info);
    gsk_render_node_types[GSK_GLYPH_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_BLUR_NODE,
      sizeof (GskBlurNode),
      NULL,
      gsk_blur_node_finalize,
      gsk_blur_node_draw,
      NULL,
      gsk_blur_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskBlurNode"), &node_info);
    gsk_render_node_types[GSK_BLUR_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_GL_SHADER_NODE,
      sizeof (GskGLShaderNode),
      NULL,
      gsk_gl_shader_node_finalize,
      gsk_gl_shader_node_draw,
      NULL,
      gsk_gl_shader_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskGLShaderNode"), &node_info);
    gsk_render_node_types[GSK_GL_SHADER_NODE] = node_type;
  }

  {
    const GskRenderNodeTypeInfo node_info =
    {
      GSK_DEBUG_NODE,
      sizeof (GskDebugNode),
      NULL,
      gsk_debug_node_finalize,
      gsk_debug_node_draw,
      gsk_debug_node_can_diff,
      gsk_debug_node_diff,
    };

    GType node_type = gsk_render_node_type_register_static (I_("GskDebugNode"), &node_info);
    gsk_render_node_types[GSK_DEBUG_NODE] = node_type;
  }
}

static void
gsk_render_node_content_serializer_finish (GObject      *source,
                                           GAsyncResult *result,
                                           gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (g_output_stream_splice_finish (stream, result, &error) < 0)
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
gsk_render_node_content_serializer (GdkContentSerializer *serializer)
{
  GInputStream *input;
  const GValue *value;
  GskRenderNode *node;
  GBytes *bytes;

  value = gdk_content_serializer_get_value (serializer);
  node = gsk_value_get_render_node (value);
  bytes = gsk_render_node_serialize (node);
  input = g_memory_input_stream_new_from_bytes (bytes);

  g_output_stream_splice_async (gdk_content_serializer_get_output_stream (serializer),
                                input,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                gdk_content_serializer_get_priority (serializer),
                                gdk_content_serializer_get_cancellable (serializer),
                                gsk_render_node_content_serializer_finish,
                                serializer);
  g_object_unref (input);
  g_bytes_unref (bytes);
}

static void
gsk_render_node_content_deserializer_finish (GObject      *source,
                              GAsyncResult *result,
                              gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;
  gssize written;
  GValue *value;
  GskRenderNode *node;
  GBytes *bytes;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (stream));

  /* For now, we ignore any parsing errors. We might want to revisit that if it turns
   * out copy/paste leads to too many errors */
  node = gsk_render_node_deserialize (bytes, NULL, NULL);

  value = gdk_content_deserializer_get_value (deserializer);
  gsk_value_take_render_node (value, node);

  gdk_content_deserializer_return_success (deserializer);
}

static void
gsk_render_node_content_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output;

  output = g_memory_output_stream_new_resizable ();

  g_output_stream_splice_async (output,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                gsk_render_node_content_deserializer_finish,
                                deserializer);
  g_object_unref (output);
}

static void
gsk_render_node_init_content_serializers (void)
{
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "application/x-gtk-render-node",
                                   gsk_render_node_content_serializer,
                                   NULL,
                                   NULL);
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "text/plain;charset=utf-8",
                                   gsk_render_node_content_serializer,
                                   NULL,
                                   NULL);
  /* The serialization format only outputs ASCII, so we can do this */
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "text/plain",
                                   gsk_render_node_content_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_deserializer ("application/x-gtk-render-node",
                                     GSK_TYPE_RENDER_NODE,
                                     gsk_render_node_content_deserializer,
                                     NULL,
                                     NULL);
}

/*< private >
 * gsk_render_node_init_types:
 *
 * Initialize all the `GskRenderNode` types provided by GSK.
 */
void
gsk_render_node_init_types (void)
{
  static gsize register_types__volatile;

  if (g_once_init_enter (&register_types__volatile))
    {
      gboolean initialized = TRUE;
      gsk_render_node_init_types_once ();
      gsk_render_node_init_content_serializers ();
      g_once_init_leave (&register_types__volatile, initialized);
    }
}

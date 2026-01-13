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

#include "gskradialgradientnodeprivate.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskcairogradientprivate.h"

#include "gdk/gdkcairoprivate.h"

G_LOCK_DEFINE_STATIC (rgba);

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
  GskGradient gradient;

  graphene_point_t start_center;
  graphene_point_t end_center;
  float start_radius;
  float end_radius;
  float aspect_ratio;
  float hradius;
};

struct _GskRepeatingRadialGradientNode
{
  GskRadialGradientNode parent;
};

static void
gsk_radial_gradient_node_finalize (GskRenderNode *node)
{
  GskRadialGradientNode *self = (GskRadialGradientNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_RADIAL_GRADIENT_NODE));

  gsk_gradient_clear (&self->gradient);

  parent_class->finalize (node);
}

static void
add_color_stop_to_pattern (float          offset,
                           GdkColorState *ccs,
                           float          values[4],
                           gpointer       data)
{
  cairo_pattern_t *pattern = data;

  cairo_pattern_add_color_stop_rgba (pattern, offset, values[0], values[1], values[2], values[3]);
}

gboolean
gsk_radial_gradient_node_is_zero_length (GskRenderNode *node)
{
  GskRadialGradientNode *self = (GskRadialGradientNode *) node;

  return self->start_radius == self->end_radius &&
         graphene_point_equal (&self->start_center, &self->end_center);
}

static void
gsk_radial_gradient_node_draw (GskRenderNode *node,
                               cairo_t       *cr,
                               GskCairoData  *data)
{
  GskRadialGradientNode *self = (GskRadialGradientNode *) node;
  cairo_pattern_t *pattern;
  gsize i;
  GskGradient *gradient = &self->gradient;
  gsize n_stops;
  float end_radius;

  if (gsk_radial_gradient_node_is_zero_length (node))
    {
      switch (gsk_gradient_get_repeat (gradient))
        {
        case GSK_REPEAT_NONE:
          return;

        case GSK_REPEAT_PAD:
          /* hack to make Cairo draw someting */
          end_radius = self->start_radius + 0.0001;
          break;

        case GSK_REPEAT_REPEAT:
        case GSK_REPEAT_REFLECT:
          {
            GdkColor color;
            gsk_gradient_get_average_color (gradient, &color);
            gdk_cairo_set_source_color (cr, data->ccs, &color);
            gdk_cairo_rect (cr, &node->bounds);
            cairo_fill (cr);
            gdk_color_finish (&color);
            return;
          }

        default:
          g_assert_not_reached ();
          return;
        }
    }
  else
    end_radius = self->end_radius;

  pattern = cairo_pattern_create_radial (0, 0, self->start_radius,
                                         self->end_center.x - self->start_center.x,
                                         self->end_center.y - self->start_center.y,
                                         end_radius);

  if (self->aspect_ratio != 1)
    {
      cairo_matrix_t matrix;

      cairo_matrix_init_scale (&matrix, 1.0, self->aspect_ratio);
      cairo_pattern_set_matrix (pattern, &matrix);
    }

  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE)
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  else
    cairo_pattern_set_extend (pattern, gsk_repeat_to_cairo (gsk_gradient_get_repeat (gradient)));

  if (gsk_gradient_get_stop_offset (gradient, 0) > 0.0)
    gdk_cairo_pattern_add_color_stop_color (pattern,
                                            data->ccs,
                                            0.0,
                                            gsk_gradient_get_stop_color (gradient, 0));

  n_stops = gsk_gradient_get_n_stops (gradient);
  for (i = 0; i < n_stops; i++)
    {
      if (!gdk_color_state_equal (gsk_gradient_get_interpolation (gradient), data->ccs))
        gsk_cairo_interpolate_color_stops (data->ccs,
                                           gsk_gradient_get_interpolation (gradient),
                                           gsk_gradient_get_hue_interpolation (gradient),
                                           i > 0 ? gsk_gradient_get_stop_offset (gradient, i - 1) : 0,
                                           i > 0 ? gsk_gradient_get_stop_color (gradient, i - 1) : gsk_gradient_get_stop_color (gradient, i),
                                           gsk_gradient_get_stop_offset (gradient, i),
                                           gsk_gradient_get_stop_color (gradient, i),
                                           i > 0 ? gsk_gradient_get_stop_transition_hint (gradient, i) : 0.5,
                                           add_color_stop_to_pattern,
                                           pattern);

      gdk_cairo_pattern_add_color_stop_color (pattern,
                                              data->ccs,
                                              gsk_gradient_get_stop_offset (gradient, i),
                                              gsk_gradient_get_stop_color (gradient, i));
    }

  if (gsk_gradient_get_stop_offset (gradient, n_stops - 1) < 1.0)
    {
      if (!gdk_color_state_equal (gsk_gradient_get_interpolation (gradient), data->ccs))
        gsk_cairo_interpolate_color_stops (data->ccs,
                                           gsk_gradient_get_interpolation (gradient),
                                           gsk_gradient_get_hue_interpolation (gradient),
                                           gsk_gradient_get_stop_offset (gradient, n_stops - 1),
                                           gsk_gradient_get_stop_color (gradient, n_stops - 1),
                                           1,
                                           gsk_gradient_get_stop_color (gradient, n_stops - 1),
                                           0.5,
                                           add_color_stop_to_pattern,
                                           pattern);

      gdk_cairo_pattern_add_color_stop_color (pattern,
                                              data->ccs,
                                              1.0,
                                              gsk_gradient_get_stop_color (gradient, n_stops - 1));
    }

  gdk_cairo_rect (cr, &node->bounds);
  cairo_translate (cr, self->start_center.x, self->start_center.y);
  cairo_set_source (cr, pattern);
  cairo_fill (cr);

  cairo_pattern_destroy (pattern);
}

static void
gsk_radial_gradient_node_diff (GskRenderNode *node1,
                               GskRenderNode *node2,
                               GskDiffData   *data)
{
  GskRadialGradientNode *self1 = (GskRadialGradientNode *) node1;
  GskRadialGradientNode *self2 = (GskRadialGradientNode *) node2;

  if (!gsk_rect_equal (&node1->bounds, &node2->bounds) ||
      !graphene_point_equal (&self1->start_center, &self2->start_center) ||
      self1->start_radius != self2->start_radius ||
      !graphene_point_equal (&self1->end_center, &self2->end_center) ||
      self1->end_radius != self2->end_radius ||
      self1->aspect_ratio != self2->aspect_ratio ||
      !gsk_gradient_equal (&self1->gradient, &self2->gradient))
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static GskRenderNode *
gsk_render_node_replay_as_self (GskRenderNode   *node,
                                GskRenderReplay *replay)
{
  return gsk_render_node_ref (node);
}

static void
gsk_radial_gradient_node_class_init (gpointer g_class,
                                     gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_RADIAL_GRADIENT_NODE;

  node_class->finalize = gsk_radial_gradient_node_finalize;
  node_class->draw = gsk_radial_gradient_node_draw;
  node_class->diff = gsk_radial_gradient_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskRadialGradientNode, gsk_radial_gradient_node)

static void
gsk_repeating_radial_gradient_node_class_init (gpointer g_class,
                                               gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_REPEATING_RADIAL_GRADIENT_NODE;

  node_class->finalize = gsk_radial_gradient_node_finalize;
  node_class->draw = gsk_radial_gradient_node_draw;
  node_class->diff = gsk_radial_gradient_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskRepeatingRadialGradientNode, gsk_repeating_radial_gradient_node)

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
  GskGradient *gradient;
  GskRenderNode *node;

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

  gradient = gsk_gradient_new ();
  gsk_gradient_add_color_stops (gradient, color_stops, n_color_stops);

  node = gsk_radial_gradient_node_new2 (bounds,
                                        center, hradius * start,
                                        center, hradius * end,
                                        hradius / vradius,
                                        gradient);

  ((GskRadialGradientNode *) node)->hradius = hradius;

  gsk_gradient_free (gradient);

  return node;
}

static gboolean
circle_contains_circle (const graphene_point_t *c1,
                        float                   r1,
                        const graphene_point_t *c2,
                        float                   r2)
{
  return graphene_point_distance (c1, c2, NULL, NULL) + r2 < r1;
}

/* If the circles are not fully contained in each other,
 * the gradient is a cone that does *not* cover the whole plane
 */
gboolean
gsk_radial_gradient_fills_plane (const graphene_point_t *c1,
                                 float                   r1,
                                 const graphene_point_t *c2,
                                 float                   r2)
{
  return circle_contains_circle (c1, r1, c2, r2) ||
         circle_contains_circle (c2, r2, c1, r1);
}

/*< private >
 * gsk_radial_gradient_node_new2:
 * @bounds: the bounds of the node
 * @start_center: the center of the start circle
 * @start_radius: the radius of the start circle
 * @end_center: the center of the end circle
 * @end_radius: the radius of the end circle
 * @aspect_ratio: the ratio by which to scale the circles vertically
 *   (around the @start_center)
 * @gradient: the gradient specification
 *
 * Creates a `GskRenderNode` that draws the radial gradient with
 * a geometry that is defined by the two circles.
 *
 * The @aspect_ratio allows turning both circles into ellipses by scaling
 * the X axis of both circles by the given amount.
 *
 * See [the SVG spec](https://www.w3.org/TR/SVG2/pservers.html#RadialGradientNotes)
 * for details about non-concentric radial gradients.
 *
 * Returns: (transfer full) (type GskRadialGradientNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_radial_gradient_node_new2 (const graphene_rect_t   *bounds,
                               const graphene_point_t  *start_center,
                               float                    start_radius,
                               const graphene_point_t  *end_center,
                               float                    end_radius,
                               float                    aspect_ratio,
                               const GskGradient       *gradient)
{
  GskRadialGradientNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (start_center != NULL, NULL);
  g_return_val_if_fail (start_radius >= 0., NULL);
  g_return_val_if_fail (end_center != NULL, NULL);
  g_return_val_if_fail (end_radius >= 0., NULL);
  g_return_val_if_fail (aspect_ratio > 0., NULL);

  if (gsk_gradient_get_repeat (gradient) == GSK_REPEAT_REPEAT)
    self = gsk_render_node_alloc (GSK_TYPE_REPEATING_RADIAL_GRADIENT_NODE);
  else
    self = gsk_render_node_alloc (GSK_TYPE_RADIAL_GRADIENT_NODE);
  node = (GskRenderNode *) self;

  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);

  graphene_point_init_from_point (&self->start_center, start_center);
  self->start_radius = start_radius;
  graphene_point_init_from_point (&self->end_center, end_center);
  self->end_radius = end_radius;
  self->aspect_ratio = aspect_ratio;
  self->hradius = end_radius;

  gsk_gradient_init_copy (&self->gradient, gradient);

  node->fully_opaque = gsk_gradient_is_opaque (gradient) &&
                       gsk_radial_gradient_fills_plane (start_center, start_radius,
                                                        end_center, end_radius);

  node->preferred_depth = gdk_color_state_get_depth (gsk_gradient_get_interpolation (gradient));
  node->is_hdr = gdk_color_state_is_hdr (gsk_gradient_get_interpolation (gradient));

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
  GskGradient *gradient;
  GskRenderNode *node;

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

  gradient = gsk_gradient_new ();
  gsk_gradient_add_color_stops (gradient, color_stops, n_color_stops);
  gsk_gradient_set_repeat (gradient, GSK_REPEAT_REPEAT);

  node = gsk_radial_gradient_node_new2 (bounds,
                                        center, hradius * start,
                                        center, hradius * end,
                                        hradius / vradius,
                                        gradient);

  ((GskRadialGradientNode *) node)->hradius = hradius;

  gsk_gradient_free (gradient);

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

  return gsk_gradient_get_n_stops (&self->gradient);
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
  GskRadialGradientNode *self = (GskRadialGradientNode *) node;
  const GskColorStop *stops;

  G_LOCK (rgba);

  stops = gsk_gradient_get_color_stops (&self->gradient, n_stops);

  G_UNLOCK (rgba);

  return stops;
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

  return &self->end_center;
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

  return self->hradius / self->aspect_ratio;
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

  return self->start_radius / self->hradius;
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

  return self->end_radius / self->hradius;
}

const graphene_point_t *
gsk_radial_gradient_node_get_start_center (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return &self->start_center;
}

const graphene_point_t *
gsk_radial_gradient_node_get_end_center (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return &self->end_center;
}

float
gsk_radial_gradient_node_get_start_radius (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return self->start_radius;
}

float
gsk_radial_gradient_node_get_end_radius (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return self->end_radius;
}

float
gsk_radial_gradient_node_get_aspect_ratio (const GskRenderNode *node)
{
  const GskRadialGradientNode *self = (const GskRadialGradientNode *) node;

  return self->aspect_ratio;
}

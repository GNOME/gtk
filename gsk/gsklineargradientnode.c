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

#include "gsklineargradientnodeprivate.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskcairogradientprivate.h"

#include "gdk/gdkcairoprivate.h"

G_LOCK_DEFINE_STATIC (rgba);

static GskRenderNode *
gsk_render_node_replay_as_self (GskRenderNode   *node,
                                GskRenderReplay *replay)
{
  return gsk_render_node_ref (node);
}

typedef struct _GskGradientNode GskGradientNode;
struct _GskGradientNode
{
  GskRenderNode render_node;
  GskGradient gradient;
};

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
  GskGradient gradient;

  graphene_point_t start;
  graphene_point_t end;
};

struct _GskRepeatingLinearGradientNode
{
  GskLinearGradientNode parent;
};

static void
gsk_linear_gradient_node_finalize (GskRenderNode *node)
{
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_LINEAR_GRADIENT_NODE));

  gsk_gradient_clear (&self->gradient);

  parent_class->finalize (node);
}

gboolean
gsk_linear_gradient_node_is_zero_length (GskRenderNode *node)
{
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;

  return graphene_point_equal (&self->start, &self->end);
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

static void
gsk_linear_gradient_node_draw (GskRenderNode *node,
                               cairo_t       *cr,
                               GskCairoData  *data)
{
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;
  GskGradient *gradient = &self->gradient;
  cairo_pattern_t *pattern;
  gsize n_stops;
  gsize i;

  if (gsk_linear_gradient_node_is_zero_length (node))
    {
      switch (gsk_gradient_get_repeat (gradient))
        {
        case GSK_REPEAT_NONE:
          return;

        case GSK_REPEAT_PAD:
          /* average first and last color stop */
          {
            GdkColor color, start, end;
            GdkColorState *interpolation = gsk_gradient_get_interpolation (&self->gradient);
            gdk_color_convert (&start,
                               interpolation,
                               gsk_gradient_get_stop_color (&self->gradient, 0));
            gdk_color_convert (&end,
                               interpolation,
                               gsk_gradient_get_stop_color (&self->gradient, gsk_gradient_get_n_stops (&self->gradient) - 1));
            gdk_color_init (&color,
                            interpolation,
                            (float[4]) { 0.5 * (start.values[0] + end.values[0]),
                                         0.5 * (start.values[1] + end.values[1]),
                                         0.5 * (start.values[2] + end.values[2]),
                                         0.5 * (start.values[3] + end.values[3]) });
            gdk_cairo_set_source_color (cr, data->ccs, &color);
            gdk_cairo_rect (cr, &node->bounds);
            cairo_fill (cr);
          }
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

  pattern = cairo_pattern_create_linear (self->start.x, self->start.y,
                                         self->end.x, self->end.y);

  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE)
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
      if (!gdk_color_state_equal (gsk_gradient_get_interpolation (gradient), data->ccs) ||
          gsk_gradient_get_stop_transition_hint (gradient, i) != 0.5)
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

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);

  gdk_cairo_rect (cr, &node->bounds);
  cairo_fill (cr);
}

static void
gsk_linear_gradient_node_diff (GskRenderNode *node1,
                               GskRenderNode *node2,
                               GskDiffData   *data)
{
  GskLinearGradientNode *self1 = (GskLinearGradientNode *) node1;
  GskLinearGradientNode *self2 = (GskLinearGradientNode *) node2;

  if (gsk_rect_equal (&node1->bounds, &node2->bounds) &&
      graphene_point_equal (&self1->start, &self2->start) &&
      graphene_point_equal (&self1->end, &self2->end) &&
      gsk_gradient_equal (&self1->gradient, &self2->gradient))
    return;

  gsk_render_node_diff_impossible (node1, node2, data);
}

static void
gsk_linear_gradient_node_class_init (gpointer g_class,
                                     gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_LINEAR_GRADIENT_NODE;

  node_class->finalize = gsk_linear_gradient_node_finalize;
  node_class->draw = gsk_linear_gradient_node_draw;
  node_class->diff = gsk_linear_gradient_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskLinearGradientNode, gsk_linear_gradient_node)

static void
gsk_repeating_linear_gradient_node_class_init (gpointer g_class,
                                               gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_REPEATING_LINEAR_GRADIENT_NODE;

  node_class->finalize = gsk_linear_gradient_node_finalize;
  node_class->draw = gsk_linear_gradient_node_draw;
  node_class->diff = gsk_linear_gradient_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskRepeatingLinearGradientNode, gsk_repeating_linear_gradient_node)

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
  GskRenderNode *node;
  GskGradient *gradient;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);

  gradient = gsk_gradient_new ();
  gsk_gradient_add_color_stops (gradient, color_stops, n_color_stops);

  node = gsk_linear_gradient_node_new2 (bounds, start, end, gradient);

  gsk_gradient_free (gradient);

  return node;
}

/*< private >
 * gsk_linear_gradient_node_new2:
 * @bounds: the rectangle to render the linear gradient into
 * @start: the point at which the linear gradient will begin
 * @end: the point at which the linear gradient will finish
 * @gradient: the gradient specification
 *
 * Creates a `GskRenderNode` that will create a linear gradient
 * from the given points and gradient specification, and render
 * that into the area given by @bounds.
 *
 * Returns: (transfer full) (type GskLinearGradientNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_linear_gradient_node_new2 (const graphene_rect_t   *bounds,
                               const graphene_point_t  *start,
                               const graphene_point_t  *end,
                               const GskGradient       *gradient)
{
  GskLinearGradientNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (gradient != NULL, NULL);

  if (gsk_gradient_get_repeat (gradient) == GSK_REPEAT_REPEAT)
    self = gsk_render_node_alloc (GSK_TYPE_REPEATING_LINEAR_GRADIENT_NODE);
  else
    self = gsk_render_node_alloc (GSK_TYPE_LINEAR_GRADIENT_NODE);
  node = (GskRenderNode *) self;

  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);
  graphene_point_init_from_point (&self->start, start);
  graphene_point_init_from_point (&self->end, end);

  gsk_gradient_init_copy (&self->gradient, gradient);

  node->fully_opaque = gsk_gradient_is_opaque (gradient);
  node->preferred_depth = gdk_color_state_get_depth (gsk_gradient_get_interpolation (gradient));
  node->is_hdr = gdk_color_state_is_hdr (gsk_gradient_get_interpolation (gradient));

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
  GskGradient *gradient;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);

  gradient = gsk_gradient_new ();
  gsk_gradient_add_color_stops (gradient, color_stops, n_color_stops);
  gsk_gradient_set_repeat (gradient, GSK_REPEAT_REPEAT);

  node = gsk_linear_gradient_node_new2 (bounds, start, end, gradient);

  gsk_gradient_free (gradient);

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

  return gsk_gradient_get_n_stops (&self->gradient);
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
  GskLinearGradientNode *self = (GskLinearGradientNode *) node;
  const GskColorStop *stops;

  G_LOCK (rgba);

  stops = gsk_gradient_get_color_stops (&self->gradient, n_stops);

  G_UNLOCK (rgba);

  return stops;
}

/*< private >
 * gsk_gradient_node_get_gradient:
 * @node: a `GskRenderNode` for a gradient
 *
 * Retrieves the gradient specification.
 *
 * Returns: the gradient specification
 */
const GskGradient *
gsk_gradient_node_get_gradient (const GskRenderNode *node)
{
  const GskGradientNode *self = (const GskGradientNode *) node;

  return &self->gradient;
}

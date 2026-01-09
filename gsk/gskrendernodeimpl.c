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
#include "gskcairoshadowprivate.h"
#include "gskcairorenderer.h"
#include "gskcolormatrixnodeprivate.h"
#include "gskcolornode.h"
#include "gskcontainernode.h"
#include "gskdebugprivate.h"
#include "gskfillnode.h"
#include "gskrectprivate.h"
#include "gskrendererprivate.h"
#include "gskrenderreplay.h"
#include "gskrepeatnode.h"
#include "gskroundedrectprivate.h"
#include "gskstrokenode.h"
#include "gsksubsurfacenode.h"
#include "gsktransformprivate.h"
#include "gskcomponenttransferprivate.h"
#include "gskprivate.h"
#include "gpu/gskglrenderer.h"

#include "gdk/gdkcairoprivate.h"
#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkprivate.h"
#include "gdk/gdkrectangleprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdktexturedownloaderprivate.h"
#include "gdk/gdkrgbaprivate.h"
#include "gdk/gdkcolorstateprivate.h"

#include <cairo.h>
#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif
#include <hb-ot.h>

/* This lock protects all on-demand created legacy rgba data of
 * render nodes.
 */
G_LOCK_DEFINE_STATIC (rgba);

/* {{{ Utilities */

static GskRenderNode *
gsk_render_node_replay_as_self (GskRenderNode   *node,
                                GskRenderReplay *replay)
{
  return gsk_render_node_ref (node);
}

/* }}} */
/* {{{ GSK_LINEAR_GRADIENT_NODE */

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

static float
adjust_hue (GskHueInterpolation  interp,
            float                h1,
            float                h2)
{
  float d;

  d = h2 - h1;
  while (d > 360)
    {
      h2 -= 360;
      d = h2 - h1;
    }
  while (d < -360)
    {
      h2 += 360;
      d = h2 - h1;
    }

  g_assert (fabsf (d) <= 360);

  switch (interp)
    {
    case GSK_HUE_INTERPOLATION_SHORTER:
      {
        if (d > 180)
          h2 -= 360;
        else if (d < -180)
          h2 += 360;
      }
      g_assert (fabsf (h2 - h1) <= 180);
      break;

    case GSK_HUE_INTERPOLATION_LONGER:
      {
        if (0 < d && d < 180)
          h2 -= 360;
        else if (-180 < d && d <= 0)
          h2 += 360;
      g_assert (fabsf (h2 - h1) >= 180);
      }
      break;

    case GSK_HUE_INTERPOLATION_INCREASING:
      if (h2 < h1)
        h2 += 360;
      d = h2 - h1;
      g_assert (h1 <= h2);
      break;

    case GSK_HUE_INTERPOLATION_DECREASING:
      if (h1 < h2)
        h2 -= 360;
      d = h2 - h1;
      g_assert (h1 >= h2);
      break;

    default:
      g_assert_not_reached ();
    }

  return h2;
}

#define lerp(t,a,b) ((a) + (t) * ((b) - (a)))

typedef void (* ColorStopCallback) (float          offset,
                                    GdkColorState *ccs,
                                    float          values[4],
                                    gpointer       data);

static void
interpolate_color_stops (GdkColorState         *ccs,
                         GdkColorState         *interpolation,
                         GskHueInterpolation    hue_interpolation,
                         float                  offset1,
                         const GdkColor        *color1,
                         float                  offset2,
                         const GdkColor        *color2,
                         float                  transition_hint,
                         ColorStopCallback      callback,
                         gpointer               data)
{
  float values1[4];
  float values2[4];
  int n;
  float exp;

  gdk_color_to_float (color1, interpolation, values1);
  gdk_color_to_float (color2, interpolation, values2);

  if (gdk_color_state_equal (interpolation, GDK_COLOR_STATE_OKLCH))
    {
      values2[2] = adjust_hue (hue_interpolation, values1[2], values2[2]);
      /* don't make hue steps larger than 30° */
      n = ceilf (fabsf (values2[2] - values1[2]) / 30);
    }
  else
    {
      /* just some steps */
      n = 7;
    }

  if (transition_hint <= 0)
    exp = 0;
  else if (transition_hint >= 1)
    exp = INFINITY;
  else if (transition_hint == 0.5)
    exp = 1;
  else
    exp = - M_LN2 / logf (transition_hint);

  for (int k = 1; k < n; k++)
    {
      float f = k / (float) n;
      float values[4];
      float offset;
      float C;
      GdkColor c;

      if (transition_hint <= 0)
        C = 1;
      else if (transition_hint >= 1)
        C = 0;
      else if (transition_hint == 0.5)
        C = f;
      else
        C = powf (f, exp);

      values[0] = lerp (C, values1[0], values2[0]);
      values[1] = lerp (C, values1[1], values2[1]);
      values[2] = lerp (C, values1[2], values2[2]);
      values[3] = lerp (C, values1[3], values2[3]);
      offset = lerp (f, offset1, offset2);

      gdk_color_init (&c, interpolation, values);
      gdk_color_to_float (&c, ccs, values);

      callback (offset, ccs, values, data);

      gdk_color_finish (&c);
    }
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

  pattern = cairo_pattern_create_linear (self->start.x, self->start.y,
                                         self->end.x, self->end.y);

  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE)
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  else
    {
      switch (gsk_gradient_get_repeat (gradient))
        {
        case GSK_REPEAT_NONE:
          cairo_pattern_set_extend (pattern, CAIRO_EXTEND_NONE);
          break;
        case GSK_REPEAT_PAD:
          cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);
          break;
        case GSK_REPEAT_REPEAT:
          cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
          break;
        case GSK_REPEAT_REFLECT:
          cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REFLECT);
          break;
        default:
          g_assert_not_reached ();
        }
    }

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
        interpolate_color_stops (data->ccs,
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
        interpolate_color_stops (data->ccs,
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

/* }}} */
/* {{{ GSK_RADIAL_GRADIENT_NODE */

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
gsk_radial_gradient_node_draw (GskRenderNode *node,
                               cairo_t       *cr,
                               GskCairoData  *data)
{
  GskRadialGradientNode *self = (GskRadialGradientNode *) node;
  cairo_pattern_t *pattern;
  gsize i;
  GskGradient *gradient = &self->gradient;
  gsize n_stops;

  pattern = cairo_pattern_create_radial (0, 0, self->start_radius,
                                         self->end_center.x - self->start_center.x,
                                         self->end_center.y - self->start_center.y,
                                         self->end_radius);

  if (self->aspect_ratio != 1)
    {
      cairo_matrix_t matrix;

      cairo_matrix_init_scale (&matrix, 1.0, self->aspect_ratio);
      cairo_pattern_set_matrix (pattern, &matrix);
    }

  if (gsk_render_node_get_node_type (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE)
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  else
    {
      switch (gsk_gradient_get_repeat (gradient))
        {
        case GSK_REPEAT_NONE:
          cairo_pattern_set_extend (pattern, CAIRO_EXTEND_NONE);
          break;
        case GSK_REPEAT_PAD:
          cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);
          break;
        case GSK_REPEAT_REPEAT:
          cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
          break;
        case GSK_REPEAT_REFLECT:
          cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REFLECT);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  if (gsk_gradient_get_stop_offset (gradient, 0) > 0.0)
    gdk_cairo_pattern_add_color_stop_color (pattern,
                                            data->ccs,
                                            0.0,
                                            gsk_gradient_get_stop_color (gradient, 0));

  n_stops = gsk_gradient_get_n_stops (gradient);
  for (i = 0; i < n_stops; i++)
    {
      if (!gdk_color_state_equal (gsk_gradient_get_interpolation (gradient), data->ccs))
        interpolate_color_stops (data->ccs,
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
        interpolate_color_stops (data->ccs,
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

/* }}} */
/* {{{ GSK_CONIC_GRADIENT_NODE */

/**
 * GskConicGradientNode:
 *
 * A render node for a conic gradient.
 */

struct _GskConicGradientNode
{
  GskRenderNode render_node;
  GskGradient gradient;

  graphene_point_t center;
  float rotation;
  float angle;
};

static void
gsk_conic_gradient_node_finalize (GskRenderNode *node)
{
  GskConicGradientNode *self = (GskConicGradientNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CONIC_GRADIENT_NODE));

  gsk_gradient_clear (&self->gradient);

  parent_class->finalize (node);
}

#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))

static void
_cairo_mesh_pattern_set_corner_rgba (cairo_pattern_t *pattern,
                                     guint            corner_num,
                                     const float      color[4])
{
  cairo_mesh_pattern_set_corner_color_rgba (pattern, corner_num, color[0], color[1], color[2], color[3]);
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
                                   const float      start_color[4],
                                   float            end_angle,
                                   const float      end_color[4])
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
add_color_stop_to_array (float    offset,
                         GdkColorState *ccs,
                         float    values[4],
                         gpointer data)
{
  GArray *stops = data;
  GskGradientStop stop;

  stop.offset = offset;
  gdk_color_init (&stop.color, ccs, values);

  g_array_append_val (stops, stop);
}

static void
gsk_conic_gradient_node_draw (GskRenderNode *node,
                              cairo_t       *cr,
                              GskCairoData  *data)
{
  GskConicGradientNode *self = (GskConicGradientNode *) node;
  cairo_pattern_t *pattern;
  graphene_point_t corner;
  float radius;
  gsize i;
  GArray *stops;
  GskGradient *gradient = &self->gradient;

  pattern = cairo_pattern_create_mesh ();
  graphene_rect_get_top_right (&node->bounds, &corner);
  radius = graphene_point_distance (&self->center, &corner, NULL, NULL);
  graphene_rect_get_bottom_right (&node->bounds, &corner);
  radius = MAX (radius, graphene_point_distance (&self->center, &corner, NULL, NULL));
  graphene_rect_get_bottom_left (&node->bounds, &corner);
  radius = MAX (radius, graphene_point_distance (&self->center, &corner, NULL, NULL));
  graphene_rect_get_top_left (&node->bounds, &corner);
  radius = MAX (radius, graphene_point_distance (&self->center, &corner, NULL, NULL));

  gsize n_stops = gsk_gradient_get_n_stops (gradient);
  const GskGradientStop *orig_stops = gsk_gradient_get_stops (gradient);

  stops = g_array_new (FALSE, TRUE, sizeof (GskGradientStop));
  g_array_set_clear_func (stops, (GDestroyNotify) clear_stop);

  if (gdk_color_state_equal (gsk_gradient_get_interpolation (gradient), data->ccs))
    {
      for (i = 0; i < n_stops; i++)
        {
          g_array_append_val (stops, orig_stops[i]);
          /* take a ref, since clear_stop removes one */
          gdk_color_state_ref (orig_stops[i].color.color_state);
        }
    }
  else
    {
      g_array_append_val (stops, orig_stops[0]);

      for (i = 1; i < n_stops; i++)
        {
          interpolate_color_stops (data->ccs,
                                   gsk_gradient_get_interpolation (gradient),
                                   gsk_gradient_get_hue_interpolation (gradient),
                                   orig_stops[i-1].offset, &orig_stops[i-1].color,
                                   orig_stops[i].offset, &orig_stops[i].color,
                                   orig_stops[i].transition_hint,
                                   add_color_stop_to_array,
                                   stops);
          g_array_append_val (stops, orig_stops[i]);
          /* take a ref, since clear_stop removes one */
          gdk_color_state_ref (orig_stops[i].color.color_state);
        }
    }

  for (i = 0; i <= stops->len; i++)
    {
      GskGradientStop *stop1 = &g_array_index (stops, GskGradientStop, MAX (i, 1) - 1);
      GskGradientStop *stop2 = &g_array_index (stops, GskGradientStop, MIN (i, stops->len - 1));
      double offset1 = i > 0 ? stop1->offset : 0;
      double offset2 = i < n_stops ? stop2->offset : 1;
      double transition_hint = i > 0 && i < n_stops ? stop2->transition_hint : 0.5;
      double start_angle, end_angle;
      float color1[4];
      float color2[4];
      double exp;

      offset1 = offset1 * 360 + self->rotation - 90;
      offset2 = offset2 * 360 + self->rotation - 90;

      gdk_color_to_float (&stop1->color, data->ccs, color1);
      gdk_color_to_float (&stop2->color, data->ccs, color2);

      if (transition_hint <= 0)
        exp = 0;
      else if (transition_hint >= 1)
        exp = INFINITY;
      else if (transition_hint == 0.5)
        exp = 1;
      else
        exp = - M_LN2 / logf (transition_hint);

      for (start_angle = offset1; start_angle < offset2; start_angle = end_angle)
        {
          float f, C;
          float start_color[4], end_color[4];

          end_angle = (floor (start_angle / 45) + 1) * 45;
          end_angle = MIN (end_angle, offset2);

          f = (start_angle - offset1) / (offset2 - offset1);
          if (transition_hint <= 0)
            C = 1;
          else if (transition_hint >= 1)
            C = 0;
          else if (transition_hint == 0.5)
            C = f;
          else
            C = powf (f, exp);

          gdk_rgba_color_interpolate ((GdkRGBA *) &start_color,
                                      (const GdkRGBA *) &color1,
                                      (const GdkRGBA *) &color2,
                                      C);

          f = (end_angle - offset1) / (offset2 - offset1);
          if (transition_hint <= 0)
            C = 1;
          else if (transition_hint >= 1)
            C = 0;
          else if (transition_hint == 0.5)
            C = f;
          else
            C = powf (f, exp);

          gdk_rgba_color_interpolate ((GdkRGBA *) &end_color,
                                      (const GdkRGBA *) &color1,
                                      (const GdkRGBA *) &color2,
                                      C);

          gsk_conic_gradient_node_add_patch (pattern,
                                             radius,
                                             DEG_TO_RAD (start_angle),
                                             start_color,
                                             DEG_TO_RAD (end_angle),
                                             end_color);
        }
    }

  g_array_unref (stops);

  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  gdk_cairo_rect (cr, &node->bounds);
  cairo_translate (cr, self->center.x, self->center.y);
  cairo_set_source (cr, pattern);
  cairo_fill (cr);

  cairo_pattern_destroy (pattern);
}

static void
gsk_conic_gradient_node_diff (GskRenderNode *node1,
                              GskRenderNode *node2,
                              GskDiffData   *data)
{
  GskConicGradientNode *self1 = (GskConicGradientNode *) node1;
  GskConicGradientNode *self2 = (GskConicGradientNode *) node2;

  if (!gsk_rect_equal (&node1->bounds, &node2->bounds) ||
      !graphene_point_equal (&self1->center, &self2->center) ||
      self1->rotation != self2->rotation ||
      !gsk_gradient_equal (&self1->gradient, &self2->gradient))
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static void
gsk_conic_gradient_node_class_init (gpointer g_class,
                                    gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_CONIC_GRADIENT_NODE;

  node_class->finalize = gsk_conic_gradient_node_finalize;
  node_class->draw = gsk_conic_gradient_node_draw;
  node_class->diff = gsk_conic_gradient_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
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
  GskGradient *gradient;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (center != NULL, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);
  g_return_val_if_fail (color_stops[0].offset >= 0, NULL);

  gradient = gsk_gradient_new ();
  gsk_gradient_add_color_stops (gradient, color_stops, n_color_stops);

  node = gsk_conic_gradient_node_new2 (bounds,
                                       center, rotation,
                                       gradient);

  gsk_gradient_free (gradient);

  return node;
}

/*< private >
 * gsk_conic_gradient_node_new2:
 * @bounds: the bounds of the node
 * @center: the center of the gradient
 * @rotation: the rotation of the gradient in degrees
 * @gradient: the gradient specification
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
gsk_conic_gradient_node_new2 (const graphene_rect_t   *bounds,
                              const graphene_point_t  *center,
                              float                    rotation,
                              const GskGradient       *gradient)
{
  GskConicGradientNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (center != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_CONIC_GRADIENT_NODE);
  node = (GskRenderNode *) self;

  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);
  graphene_point_init_from_point (&self->center, center);

  self->rotation = rotation;

  gsk_gradient_init_copy (&self->gradient, gradient);

  node->fully_opaque = gsk_gradient_is_opaque (gradient);
  node->preferred_depth = gdk_color_state_get_depth (gsk_gradient_get_interpolation (gradient));
  node->is_hdr = gdk_color_state_is_hdr (gsk_gradient_get_interpolation (gradient));

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

  return gsk_gradient_get_n_stops (&self->gradient);
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
  GskConicGradientNode *self = (GskConicGradientNode *) node;
  const GskColorStop *stops;

  G_LOCK (rgba);

  stops = gsk_gradient_get_color_stops (&self->gradient, n_stops);

  G_UNLOCK (rgba);

  return stops;
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

/* }}} */
/* {{{ GSK_INSET_SHADOW_NODE */

/**
 * GskInsetShadowNode:
 *
 * A render node for an inset shadow.
 */
struct _GskInsetShadowNode
{
  GskRenderNode render_node;

  GskRoundedRect outline;
  GdkColor color;
  graphene_point_t offset;
  float spread;
  float blur_radius;
};

static void
gsk_inset_shadow_node_finalize (GskRenderNode *node)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_INSET_SHADOW_NODE));

  gdk_color_finish (&self->color);

  parent_class->finalize (node);
}

static void
gsk_inset_shadow_node_draw (GskRenderNode *node,
                            cairo_t       *cr,
                            GskCairoData  *data)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;
  GskRoundedRect box, clip_box;
  int clip_radius;
  graphene_rect_t clip_rect;
  double blur_radius;

  /* We don't need to draw invisible shadows */
  if (gdk_color_is_clear (&self->color))
    return;

  _graphene_rect_init_from_clip_extents (&clip_rect, cr);
  if (!gsk_rounded_rect_intersects_rect (&self->outline, &clip_rect))
    return;

  blur_radius = self->blur_radius / 2;

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius);

  cairo_save (cr);

  gsk_rounded_rect_path (&self->outline, cr);
  cairo_clip (cr);

  gsk_rounded_rect_init_copy (&box, &self->outline);
  gsk_rounded_rect_offset (&box, self->offset.x, self->offset.y);
  gsk_rounded_rect_shrink (&box, self->spread, self->spread, self->spread, self->spread);

  gsk_rounded_rect_init_copy (&clip_box, &self->outline);
  gsk_rounded_rect_shrink (&clip_box, -clip_radius, -clip_radius, -clip_radius, -clip_radius);

  if (!gsk_cairo_shadow_needs_blur (blur_radius))
    {
      gsk_cairo_shadow_draw (cr, data->ccs, TRUE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
    }
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
      gsk_rect_to_cairo_grow (&clip_box.bounds, &r);
      remaining = cairo_region_create_rectangle (&r);

      /* First do the corners of box */
      for (i = 0; i < 4; i++)
        {
          cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
          gsk_cairo_shadow_draw_corner (cr, data->ccs, TRUE, &box, &clip_box, blur_radius, &self->color, i, &r);
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
          gsk_cairo_shadow_draw_side (cr, data->ccs, TRUE, &box, &clip_box, blur_radius, &self->color, i, &r);
          cairo_restore (cr);

          /* We drew the region, remove it from remaining */
          cairo_region_subtract_rectangle (remaining, &r);
        }

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      gsk_cairo_shadow_draw (cr, data->ccs, TRUE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
      cairo_restore (cr);

      cairo_region_destroy (remaining);
    }

  cairo_restore (cr);
}

static void
gsk_inset_shadow_node_diff (GskRenderNode *node1,
                            GskRenderNode *node2,
                            GskDiffData   *data)
{
  GskInsetShadowNode *self1 = (GskInsetShadowNode *) node1;
  GskInsetShadowNode *self2 = (GskInsetShadowNode *) node2;

  if (gsk_rounded_rect_equal (&self1->outline, &self2->outline) &&
      gdk_color_equal (&self1->color, &self2->color) &&
      graphene_point_equal (&self1->offset, &self2->offset) &&
      self1->spread == self2->spread &&
      self1->blur_radius == self2->blur_radius)
    return;

  gsk_render_node_diff_impossible (node1, node2, data);
}

static void
gsk_inset_shadow_node_class_init (gpointer g_class,
                                  gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_INSET_SHADOW_NODE;

  node_class->finalize = gsk_inset_shadow_node_finalize;
  node_class->draw = gsk_inset_shadow_node_draw;
  node_class->diff = gsk_inset_shadow_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
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
  GdkColor color2;
  GskRenderNode *node;

  gdk_color_init_from_rgba (&color2, color);
  node = gsk_inset_shadow_node_new2 (outline,
                                     &color2,
                                     &GRAPHENE_POINT_INIT (dx, dy),
                                     spread, blur_radius);
  gdk_color_finish (&color2);

  return node;
}

/*< private >
 * gsk_inset_shadow_node_new2:
 * @outline: outline of the region containing the shadow
 * @color: color of the shadow
 * @offset: offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a `GskRenderNode` that will render an inset shadow
 * into the box given by @outline.
 *
 * Returns: (transfer full) (type GskInsetShadowNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_inset_shadow_node_new2 (const GskRoundedRect   *outline,
                            const GdkColor         *color,
                            const graphene_point_t *offset,
                            float                   spread,
                            float                   blur_radius)
{
  GskInsetShadowNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (color != NULL, NULL);
  g_return_val_if_fail (offset != NULL, NULL);
  g_return_val_if_fail (blur_radius >= 0, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_INSET_SHADOW_NODE);
  node = (GskRenderNode *) self;
  node->preferred_depth = GDK_MEMORY_NONE;

  gsk_rounded_rect_init_copy (&self->outline, outline);
  gdk_color_init_copy (&self->color, color);
  self->offset = *offset;
  self->spread = spread;
  self->blur_radius = blur_radius;

  gsk_rect_init_from_rect (&node->bounds, &self->outline.bounds);

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
 * The value returned by this function will not be correct
 * if the render node was created for a non-sRGB color.
 *
 * Returns: (transfer none): the color of the shadow
 */
const GdkRGBA *
gsk_inset_shadow_node_get_color (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  /* NOTE: This is only correct for nodes with sRGB colors */
  return (const GdkRGBA *) &self->color.values;
}

/*< private >
 * gsk_inset_shadow_node_get_gdk_color:
 * @node: (type GskInsetShadowNode): a `GskRenderNode`
 *
 * Retrieves the color of the given @node.
 *
 * Returns: (transfer none): the color of the node
 */
const GdkColor *
gsk_inset_shadow_node_get_gdk_color (const GskRenderNode *node)
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

  return self->offset.x;
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

  return self->offset.y;
}

/**
 * gsk_inset_shadow_node_get_offset:
 * @node: (type GskInsetShadowNode): a `GskRenderNode` for an inset shadow
 *
 * Retrieves the offset of the inset shadow.
 *
 * Returns: an offset, in pixels
 **/
const graphene_point_t *
gsk_inset_shadow_node_get_offset (const GskRenderNode *node)
{
  const GskInsetShadowNode *self = (const GskInsetShadowNode *) node;

  return &self->offset;
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

/* }}} */
/* {{{ GSK_OUTSET_SHADOW_NODE */

/**
 * GskOutsetShadowNode:
 *
 * A render node for an outset shadow.
 */
struct _GskOutsetShadowNode
{
  GskRenderNode render_node;

  GskRoundedRect outline;
  GdkColor color;
  graphene_point_t offset;
  float spread;
  float blur_radius;
};

static void
gsk_outset_shadow_node_finalize (GskRenderNode *node)
{
  GskInsetShadowNode *self = (GskInsetShadowNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_OUTSET_SHADOW_NODE));

  gdk_color_finish (&self->color);

  parent_class->finalize (node);
}

static void
gsk_outset_shadow_get_extents (GskOutsetShadowNode *self,
                               float               *top,
                               float               *right,
                               float               *bottom,
                               float               *left)
{
  float clip_radius;

  clip_radius = gsk_cairo_blur_compute_pixels (ceil (self->blur_radius / 2.0));
  *top = MAX (0, ceil (clip_radius + self->spread - self->offset.y));
  *right = MAX (0, ceil (clip_radius + self->spread + self->offset.x));
  *bottom = MAX (0, ceil (clip_radius + self->spread + self->offset.y));
  *left = MAX (0, ceil (clip_radius + self->spread - self->offset.x));
}

static void
gsk_outset_shadow_node_draw (GskRenderNode *node,
                             cairo_t       *cr,
                             GskCairoData  *data)
{
  GskOutsetShadowNode *self = (GskOutsetShadowNode *) node;
  GskRoundedRect box, clip_box;
  int clip_radius;
  graphene_rect_t clip_rect;
  float top, right, bottom, left;
  double blur_radius;

  /* We don't need to draw invisible shadows */
  if (gdk_color_is_clear (&self->color))
    return;

  _graphene_rect_init_from_clip_extents (&clip_rect, cr);
  if (!gsk_rounded_rect_intersects_rect (&self->outline, &clip_rect))
    return;

  blur_radius = self->blur_radius / 2;

  clip_radius = gsk_cairo_blur_compute_pixels (blur_radius);

  cairo_save (cr);

  gsk_rounded_rect_init_copy (&clip_box, &self->outline);
  gsk_outset_shadow_get_extents (self, &top, &right, &bottom, &left);
  gsk_rounded_rect_shrink (&clip_box, -top, -right, -bottom, -left);

  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
  gsk_rounded_rect_path (&self->outline, cr);
  gdk_cairo_rect (cr, &clip_box.bounds);

  cairo_clip (cr);

  gsk_rounded_rect_init_copy (&box, &self->outline);
  gsk_rounded_rect_offset (&box, self->offset.x, self->offset.y);
  gsk_rounded_rect_shrink (&box, -self->spread, -self->spread, -self->spread, -self->spread);

  if (!gsk_cairo_shadow_needs_blur (blur_radius))
    {
      gsk_cairo_shadow_draw (cr, data->ccs, FALSE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
    }
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
          gsk_cairo_shadow_draw_corner (cr, data->ccs, FALSE, &box, &clip_box, blur_radius, &self->color, i, &r);
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
          gsk_cairo_shadow_draw_side (cr, data->ccs, FALSE, &box, &clip_box, blur_radius, &self->color, i, &r);
          cairo_restore (cr);

          /* We drew the region, remove it from remaining */
          cairo_region_subtract_rectangle (remaining, &r);
        }

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      gsk_cairo_shadow_draw (cr, data->ccs, FALSE, &box, &clip_box, blur_radius, &self->color, GSK_BLUR_NONE);
      cairo_restore (cr);

      cairo_region_destroy (remaining);
    }

  cairo_restore (cr);
}

static void
gsk_outset_shadow_node_diff (GskRenderNode *node1,
                             GskRenderNode *node2,
                             GskDiffData   *data)
{
  GskOutsetShadowNode *self1 = (GskOutsetShadowNode *) node1;
  GskOutsetShadowNode *self2 = (GskOutsetShadowNode *) node2;

  if (gsk_rounded_rect_equal (&self1->outline, &self2->outline) &&
      gdk_color_equal (&self1->color, &self2->color) &&
      graphene_point_equal (&self1->offset, &self2->offset) &&
      self1->spread == self2->spread &&
      self1->blur_radius == self2->blur_radius)
    return;

  gsk_render_node_diff_impossible (node1, node2, data);
}

static void
gsk_outset_shadow_node_class_init (gpointer g_class,
                                   gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_OUTSET_SHADOW_NODE;

  node_class->finalize = gsk_outset_shadow_node_finalize;
  node_class->draw = gsk_outset_shadow_node_draw;
  node_class->diff = gsk_outset_shadow_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
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
  GdkColor color2;
  GskRenderNode *node;

  gdk_color_init_from_rgba (&color2, color);
  node = gsk_outset_shadow_node_new2 (outline,
                                      &color2,
                                      &GRAPHENE_POINT_INIT (dx, dy),
                                      spread, blur_radius);
  gdk_color_finish (&color2);

  return node;
}

/*< private >
 * gsk_outset_shadow_node_new2:
 * @outline: outline of the region surrounded by shadow
 * @color: color of the shadow
 * @offset: offset of shadow
 * @spread: how far the shadow spreads towards the inside
 * @blur_radius: how much blur to apply to the shadow
 *
 * Creates a `GskRenderNode` that will render an outset shadow
 * around the box given by @outline.
 *
 * Returns: (transfer full) (type GskOutsetShadowNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_outset_shadow_node_new2 (const GskRoundedRect   *outline,
                             const GdkColor         *color,
                             const graphene_point_t *offset,
                             float                   spread,
                             float                   blur_radius)
{
  GskOutsetShadowNode *self;
  GskRenderNode *node;
  float top, right, bottom, left;

  g_return_val_if_fail (outline != NULL, NULL);
  g_return_val_if_fail (color != NULL, NULL);
  g_return_val_if_fail (blur_radius >= 0, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_OUTSET_SHADOW_NODE);
  node = (GskRenderNode *) self;
  node->preferred_depth = GDK_MEMORY_NONE;

  gsk_rounded_rect_init_copy (&self->outline, outline);
  gdk_color_init_copy (&self->color, color);
  self->offset = *offset;
  self->spread = spread;
  self->blur_radius = blur_radius;

  gsk_outset_shadow_get_extents (self, &top, &right, &bottom, &left);

  gsk_rect_init_from_rect (&node->bounds, &self->outline.bounds);
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
 * The value returned by this function will not be correct
 * if the render node was created for a non-sRGB color.
 *
 * Returns: (transfer none): a color
 */
const GdkRGBA *
gsk_outset_shadow_node_get_color (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  /* NOTE: This is only correct for nodes with sRGB colors */
  return (const GdkRGBA *) &self->color.values;
}

/*< private >
 * gsk_outset_shadow_node_get_gdk_color:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode`
 *
 * Retrieves the color of the given @node.
 *
 * Returns: (transfer none): the color of the node
 */
const GdkColor *
gsk_outset_shadow_node_get_gdk_color (const GskRenderNode *node)
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

  return self->offset.x;
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

  return self->offset.y;
}

/**
 * gsk_outset_shadow_node_get_offset:
 * @node: (type GskOutsetShadowNode): a `GskRenderNode` for an outset shadow
 *
 * Retrieves the offset of the outset shadow.
 *
 * Returns: an offset, in pixels
 **/
const graphene_point_t *
gsk_outset_shadow_node_get_offset (const GskRenderNode *node)
{
  const GskOutsetShadowNode *self = (const GskOutsetShadowNode *) node;

  return &self->offset;
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

/* }}} */
/* {{{ Serialization */

static void
gsk_render_node_serialize_bytes_finish (GObject      *source,
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
gsk_render_node_serialize_bytes (GdkContentSerializer *serializer,
                                 GBytes               *bytes)
{
  GInputStream *input;

  input = g_memory_input_stream_new_from_bytes (bytes);

  g_output_stream_splice_async (gdk_content_serializer_get_output_stream (serializer),
                                input,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                gdk_content_serializer_get_priority (serializer),
                                gdk_content_serializer_get_cancellable (serializer),
                                gsk_render_node_serialize_bytes_finish,
                                serializer);
  g_object_unref (input);
  g_bytes_unref (bytes);
}

#ifdef CAIRO_HAS_SVG_SURFACE
static cairo_status_t
gsk_render_node_cairo_serializer_write (gpointer             user_data,
                                        const unsigned char *data,
                                        unsigned int         length)
{
  g_byte_array_append (user_data, data, length);

  return CAIRO_STATUS_SUCCESS;
}

static void
gsk_render_node_svg_serializer (GdkContentSerializer *serializer)
{
  GskRenderNode *node;
  cairo_surface_t *surface;
  cairo_t *cr;
  graphene_rect_t bounds;
  GByteArray *array;

  node = gsk_value_get_render_node (gdk_content_serializer_get_value (serializer));
  gsk_render_node_get_bounds (node, &bounds);
  array = g_byte_array_new ();

  surface = cairo_svg_surface_create_for_stream (gsk_render_node_cairo_serializer_write,
                                                 array,
                                                 bounds.size.width,
                                                 bounds.size.height);
  cairo_svg_surface_set_document_unit (surface, CAIRO_SVG_UNIT_PX);
  cairo_surface_set_device_offset (surface, -bounds.origin.x, -bounds.origin.y);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  cairo_surface_finish (surface);
  if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
      gsk_render_node_serialize_bytes (serializer, g_byte_array_free_to_bytes (array));
    }
  else
    {
      GError *error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                                           cairo_status_to_string (cairo_surface_status (surface)));
      gdk_content_serializer_return_error (serializer, error);
      g_byte_array_unref (array);
    }
  cairo_surface_destroy (surface);
}
#endif

static void
gsk_render_node_png_serializer (GdkContentSerializer *serializer)
{
  GskRenderNode *node;
  GdkTexture *texture;
  GskRenderer *renderer;
  GBytes *bytes;

  node = gsk_value_get_render_node (gdk_content_serializer_get_value (serializer));

  renderer = gsk_gl_renderer_new ();
  if (!gsk_renderer_realize (renderer, NULL, NULL))
    {
      g_object_unref (renderer);
      renderer = gsk_cairo_renderer_new ();
      if (!gsk_renderer_realize (renderer, NULL, NULL))
        {
          g_assert_not_reached ();
        }
    }
  texture = gsk_renderer_render_texture (renderer, node, NULL);
  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);

  bytes = gdk_texture_save_to_png_bytes (texture);
  g_object_unref (texture);

  gsk_render_node_serialize_bytes (serializer, bytes);
}

static void
gsk_render_node_content_serializer (GdkContentSerializer *serializer)
{
  const GValue *value;
  GskRenderNode *node;
  GBytes *bytes;

  value = gdk_content_serializer_get_value (serializer);
  node = gsk_value_get_render_node (value);
  bytes = gsk_render_node_serialize (node);

  gsk_render_node_serialize_bytes (serializer, bytes);
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
#ifdef CAIRO_HAS_SVG_SURFACE
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "image/svg+xml",
                                   gsk_render_node_svg_serializer,
                                   NULL,
                                   NULL);
#endif
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "image/png",
                                   gsk_render_node_png_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_deserializer ("application/x-gtk-render-node",
                                     GSK_TYPE_RENDER_NODE,
                                     gsk_render_node_content_deserializer,
                                     NULL,
                                     NULL);
}

/* }}} */
/* {{{ Type registration */

GSK_DEFINE_RENDER_NODE_TYPE (GskLinearGradientNode, gsk_linear_gradient_node)
GSK_DEFINE_RENDER_NODE_TYPE (GskRepeatingLinearGradientNode, gsk_repeating_linear_gradient_node)
GSK_DEFINE_RENDER_NODE_TYPE (GskRadialGradientNode, gsk_radial_gradient_node)
GSK_DEFINE_RENDER_NODE_TYPE (GskRepeatingRadialGradientNode, gsk_repeating_radial_gradient_node)
GSK_DEFINE_RENDER_NODE_TYPE (GskConicGradientNode, gsk_conic_gradient_node)
GSK_DEFINE_RENDER_NODE_TYPE (GskInsetShadowNode, gsk_inset_shadow_node)
GSK_DEFINE_RENDER_NODE_TYPE (GskOutsetShadowNode, gsk_outset_shadow_node)

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
      gsk_render_node_init_content_serializers ();
      g_once_init_leave (&register_types__volatile, initialized);
    }
}

/* vim:set foldmethod=marker: */

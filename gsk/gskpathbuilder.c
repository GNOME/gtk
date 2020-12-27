/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gskpathbuilder.h"

#include "gskpathprivate.h"

/**
 * GskPathBuilder:
 *
 * A `GskPathBuilder` is an auxiliary object that is used to
 * create new `GskPath` objects.
 *
 * A path is constructed like this:
 *
 * ```
 * GskPath *
 * construct_path (void)
 * {
 *   GskPathBuilder *builder;
 *
 *   builder = gsk_path_builder_new ();
 *
 *   // add contours to the path here
 *
 *   return gsk_path_builder_free_to_path (builder);
 * }
 * ```
 *
 * Adding contours to the path can be done in two ways.
 * The easiest option is to use the `gsk_path_builder_add_*` group
 * of functions that add predefined contours to the current path,
 * either common shapes like [method@Gsk.PathBuilder.add_circle]
 * or by adding from other paths like [method@Gsk.PathBuilder.add_path].
 *
 * The other option is to define each line and curve manually with
 * the `gsk_path_builder_*_to` group of functions. You start with
 * a call to [method@Gsk.PathBuilder.move_to] to set the starting point
 * and then use multiple calls to any of the drawing functions to
 * move the pen along the plane. Once you are done, you can call
 * [method@Gsk.PathBuilder.close] to close the path by connecting it
 * back with a line to the starting point.
 * This is similar for how paths are drawn in Cairo.
 */

struct _GskPathBuilder
{
  int ref_count;

  GSList *contours; /* (reverse) list of already recorded contours */

  GskPathFlags flags; /* flags for the current path */
  graphene_point_t current_point; /* the point all drawing ops start from */
  GArray *ops; /* operations for current contour - size == 0 means no current contour */
  GArray *points; /* points for the operations */
};

G_DEFINE_BOXED_TYPE (GskPathBuilder,
                     gsk_path_builder,
                     gsk_path_builder_ref,
                     gsk_path_builder_unref)


/**
 * gsk_path_builder_new:
 *
 * Create a new `GskPathBuilder` object. The resulting builder
 * would create an empty `GskPath`. Use addition functions to add
 * types to it.
 *
 * Returns: a new `GskPathBuilder`
 **/
GskPathBuilder *
gsk_path_builder_new (void)
{
  GskPathBuilder *builder;

  builder = g_slice_new0 (GskPathBuilder);
  builder->ref_count = 1;

  builder->ops = g_array_new (FALSE, FALSE, sizeof (gskpathop));
  builder->points = g_array_new (FALSE, FALSE, sizeof (graphene_point_t));

  /* Be explicit here */
  builder->current_point = GRAPHENE_POINT_INIT (0, 0);

  return builder;
}

/**
 * gsk_path_builder_ref:
 * @builder: a `GskPathBuilder`
 *
 * Acquires a reference on the given @builder.
 *
 * This function is intended primarily for bindings. `GskPathBuilder` objects
 * should not be kept around.
 *
 * Returns: (transfer none): the given `GskPathBuilder` with
 *   its reference count increased
 */
GskPathBuilder *
gsk_path_builder_ref (GskPathBuilder *builder)
{
  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (builder->ref_count > 0, NULL);

  builder->ref_count += 1;

  return builder;
}

/* We're cheating here. Out pathops are relative to the NULL pointer,
 * so that we can not care about the points GArray reallocating itself
 * until we create the contour.
 * This does however mean that we need to not use gsk_pathop_get_points()
 * without offsetting the returned pointer.
 */
static inline gskpathop
gsk_pathop_encode_index (GskPathOperation op,
                         gsize            index)
{
  return gsk_pathop_encode (op, ((graphene_point_t *) NULL) + index);
}

static void
gsk_path_builder_ensure_current (GskPathBuilder *builder)
{
  if (builder->ops->len != 0)
    return;

  builder->flags = GSK_PATH_FLAT;
  g_array_append_vals (builder->ops, (gskpathop[1]) { gsk_pathop_encode_index (GSK_PATH_MOVE, 0) }, 1);
  g_array_append_val (builder->points, builder->current_point);
}

static void
gsk_path_builder_append_current (GskPathBuilder         *builder,
                                 GskPathOperation        op,
                                 gsize                   n_points,
                                 const graphene_point_t *points)
{
  gsk_path_builder_ensure_current (builder);

  g_array_append_vals (builder->ops, (gskpathop[1]) { gsk_pathop_encode_index (op, builder->points->len - 1) }, 1);
  g_array_append_vals (builder->points, points, n_points);

  builder->current_point = points[n_points - 1];
}

static void
gsk_path_builder_end_current (GskPathBuilder *builder)
{
  GskContour *contour;

  if (builder->ops->len == 0)
   return;

  contour = gsk_standard_contour_new (builder->flags,
                                      (graphene_point_t *) builder->points->data,
                                      builder->points->len,
                                      (gskpathop *) builder->ops->data,
                                      builder->ops->len,
                                      (graphene_point_t *) builder->points->data - (graphene_point_t *) NULL);

  g_array_set_size (builder->ops, 0);
  g_array_set_size (builder->points, 0);

  /* do this at the end to avoid inflooping when add_contour calls back here */
  gsk_path_builder_add_contour (builder, contour);
}

static void
gsk_path_builder_clear (GskPathBuilder *builder)
{
  gsk_path_builder_end_current (builder);

  g_slist_free_full (builder->contours, g_free);
  builder->contours = NULL;
}

/**
 * gsk_path_builder_unref:
 * @builder: a `GskPathBuilder`
 *
 * Releases a reference on the given @builder.
 */
void
gsk_path_builder_unref (GskPathBuilder *builder)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (builder->ref_count > 0);

  builder->ref_count -= 1;

  if (builder->ref_count > 0)
    return;

  gsk_path_builder_clear (builder);
  g_array_unref (builder->ops);
  g_array_unref (builder->points);
  g_slice_free (GskPathBuilder, builder);
}

/**
 * gsk_path_builder_free_to_path: (skip)
 * @builder: a `GskPathBuilder`
 *
 * Creates a new `GskPath` from the current state of the
 * given @builder, and frees the @builder instance.
 *
 * Returns: (transfer full): the newly created `GskPath`
 *   with all the contours added to @builder
 */
GskPath *
gsk_path_builder_free_to_path (GskPathBuilder *builder)
{
  GskPath *res;

  g_return_val_if_fail (builder != NULL, NULL);

  res = gsk_path_builder_to_path (builder);

  gsk_path_builder_unref (builder);

  return res;
}

/**
 * gsk_path_builder_to_path:
 * @builder: a `GskPathBuilder`
 *
 * Creates a new `GskPath` from the given @builder.
 *
 * The given `GskPathBuilder` is reset once this function returns;
 * you cannot call this function multiple times on the same @builder instance.
 *
 * This function is intended primarily for bindings. C code should use
 * gsk_path_builder_free_to_path().
 *
 * Returns: (transfer full): the newly created `GskPath`
 *   with all the contours added to @builder
 */
GskPath *
gsk_path_builder_to_path (GskPathBuilder *builder)
{
  GskPath *path;

  g_return_val_if_fail (builder != NULL, NULL);

  gsk_path_builder_end_current (builder);

  builder->contours = g_slist_reverse (builder->contours);

  path = gsk_path_new_from_contours (builder->contours);

  gsk_path_builder_clear (builder);

  return path;
}

void
gsk_path_builder_add_contour (GskPathBuilder *builder,
                              GskContour     *contour)
{
  gsk_path_builder_end_current (builder);

  builder->contours = g_slist_prepend (builder->contours, contour);
}

/**
 * gsk_path_builder_get_current_point:
 * @builder: a #GskPathBuilder
 *
 * Gets the current point. The current point is used for relative
 * drawing commands and updated after every operation.
 *
 * When @builder is created, the default current point is set to (0, 0).
 *
 * Returns: (transfer none): The current point
 **/
const graphene_point_t *
gsk_path_builder_get_current_point (GskPathBuilder *builder)
{
  g_return_val_if_fail (builder != NULL, NULL);

  return &builder->current_point;
}

/**
 * gsk_path_builder_add_path:
 * @builder: a `GskPathBuilder`
 * @path: (transfer none): the path to append
 *
 * Appends all of @path to @builder.
 **/
void
gsk_path_builder_add_path (GskPathBuilder *builder,
                           GskPath        *path)
{
  gsize i;

  g_return_if_fail (builder != NULL);
  g_return_if_fail (path != NULL);

  for (i = 0; i < gsk_path_get_n_contours (path); i++)
    {
      const GskContour *contour = gsk_path_get_contour (path, i);

      gsk_path_builder_add_contour (builder, gsk_contour_dup (contour));
    }
}

/**
 * gsk_path_builder_add_rect:
 * @builder: A `GskPathBuilder`
 * @rect: The rectangle to add to @builder
 *
 * Adds a path representing the given rectangle.
 *
 * If the width or height of the rectangle is negative, the start
 * point will be on the right or bottom, respectively.
 *
 * If the the width or height are 0, the path will be a closed
 * horizontal or vertical line. If both are 0, it'll be a closed dot.
 **/
void
gsk_path_builder_add_rect (GskPathBuilder        *builder,
                           const graphene_rect_t *rect)
{
  GskContour *contour;

  g_return_if_fail (builder != NULL);

  contour = gsk_rect_contour_new (rect);
  gsk_path_builder_add_contour (builder, contour);

  gsk_contour_get_start_end (contour, NULL, &builder->current_point);
}

/**
 * gsk_path_builder_add_rounded_rect:
 * @builder: a `GskPathBuilder`
 * @rect: the rounded rect
 *
 * Adds @rect as a new contour to the path built in @builder.
 **/
void
gsk_path_builder_add_rounded_rect (GskPathBuilder       *builder,
                                   const GskRoundedRect *rect)
{
  const float weight = sqrt(0.5f);

  g_return_if_fail (builder != NULL);
  g_return_if_fail (rect != NULL);

  gsk_path_builder_move_to (builder,
                            rect->bounds.origin.x + rect->corner[GSK_CORNER_TOP_LEFT].width,
                            rect->bounds.origin.y);
  /* top */
  gsk_path_builder_line_to (builder,
                            rect->bounds.origin.x + rect->bounds.size.width - rect->corner[GSK_CORNER_TOP_RIGHT].width,
                            rect->bounds.origin.y);
  /* topright corner */
  gsk_path_builder_conic_to (builder,
                             rect->bounds.origin.x + rect->bounds.size.width,
                             rect->bounds.origin.y,
                             rect->bounds.origin.x + rect->bounds.size.width,
                             rect->bounds.origin.y + rect->corner[GSK_CORNER_TOP_RIGHT].height,
                             weight);
  /* right */
  gsk_path_builder_line_to (builder,
                            rect->bounds.origin.x + rect->bounds.size.width,
                            rect->bounds.origin.y + rect->bounds.size.height - rect->corner[GSK_CORNER_BOTTOM_RIGHT].height);
  /* bottomright corner */
  gsk_path_builder_conic_to (builder,
                             rect->bounds.origin.x + rect->bounds.size.width,
                             rect->bounds.origin.y + rect->bounds.size.height,
                             rect->bounds.origin.x + rect->bounds.size.width - rect->corner[GSK_CORNER_BOTTOM_RIGHT].width,
                             rect->bounds.origin.y + rect->bounds.size.height,
                             weight);
  /* bottom */
  gsk_path_builder_line_to (builder,
                            rect->bounds.origin.x + rect->corner[GSK_CORNER_BOTTOM_LEFT].width,
                            rect->bounds.origin.y + rect->bounds.size.height);
  /* bottomleft corner */
  gsk_path_builder_conic_to (builder,
                             rect->bounds.origin.x,
                             rect->bounds.origin.y + rect->bounds.size.height,
                             rect->bounds.origin.x,
                             rect->bounds.origin.y + rect->bounds.size.height - rect->corner[GSK_CORNER_BOTTOM_LEFT].height,
                             weight);
  /* left */
  gsk_path_builder_line_to (builder,
                            rect->bounds.origin.x,
                            rect->bounds.origin.y + rect->corner[GSK_CORNER_TOP_LEFT].height);
  /* topleft corner */
  gsk_path_builder_conic_to (builder,
                             rect->bounds.origin.x,
                             rect->bounds.origin.y,
                             rect->bounds.origin.x + rect->corner[GSK_CORNER_TOP_LEFT].width,
                             rect->bounds.origin.y,
                             weight);
  /* done */
  gsk_path_builder_close (builder);
}

/**
 * gsk_path_builder_add_circle:
 * @builder: a `GskPathBuilder`
 * @center: the center of the circle
 * @radius: the radius of the circle
 *
 * Adds a circle with the @center and @radius.
 **/
void
gsk_path_builder_add_circle (GskPathBuilder         *builder,
                             const graphene_point_t *center,
                             float                   radius)
{
  GskContour *contour;

  g_return_if_fail (builder != NULL);
  g_return_if_fail (center != NULL);
  g_return_if_fail (radius > 0);

  contour = gsk_circle_contour_new (center, radius, 0, 360);
  gsk_path_builder_add_contour (builder, contour);
}

/**
 * gsk_path_builder_add_ellipse:
 * @builder: a `GskPathBuilder`
 * @center: the center point of the ellipse
 * @radius: the radius of the ellipse in x/y direction
 *
 * Adds an ellipse with the given @center and the @radius in
 * x/y direction.
 **/
void
gsk_path_builder_add_ellipse (GskPathBuilder         *builder,
                              const graphene_point_t *center,
                              const graphene_size_t  *radius)
{
  const float weight = sqrt(0.5f);
  graphene_point_t pts[8];

  g_return_if_fail (builder != NULL);
  g_return_if_fail (center != NULL);
  g_return_if_fail (radius != NULL);

  pts[0] = GRAPHENE_POINT_INIT (center->x + radius->width / 2,
                                center->y);
  pts[1] = GRAPHENE_POINT_INIT (center->x + radius->width / 2,
                                center->y + radius->height / 2);
  pts[2] = GRAPHENE_POINT_INIT (center->x,
                                center->y + radius->height / 2);
  pts[3] = GRAPHENE_POINT_INIT (center->x - radius->width / 2,
                                center->y + radius->height / 2);
  pts[4] = GRAPHENE_POINT_INIT (center->x - radius->width / 2,
                                center->y);
  pts[5] = GRAPHENE_POINT_INIT (center->x - radius->width / 2,
                                center->y - radius->height / 2);
  pts[6] = GRAPHENE_POINT_INIT (center->x,
                                center->y - radius->height / 2);
  pts[7] = GRAPHENE_POINT_INIT (center->x + radius->width / 2,
                                center->y - radius->height / 2);

  gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
  gsk_path_builder_conic_to (builder, pts[1].x, pts[1].y, pts[2].x, pts[2].y, weight);
  gsk_path_builder_conic_to (builder, pts[3].x, pts[3].y, pts[4].x, pts[4].y, weight);
  gsk_path_builder_conic_to (builder, pts[5].x, pts[5].y, pts[6].x, pts[6].y, weight);
  gsk_path_builder_conic_to (builder, pts[7].x, pts[7].y, pts[0].x, pts[0].y, weight);
  gsk_path_builder_close (builder);
}

/**
 * gsk_path_builder_move_to:
 * @builder: a `GskPathBuilder`
 * @x: x coordinate
 * @y: y coordinate
 *
 * Starts a new contour by placing the pen at @x, @y.
 *
 * If `gsk_path_builder_move_to()` is called twice in succession,
 * the first call will result in a contour made up of a single point.
 * The second call will start a new contour.
 **/
void
gsk_path_builder_move_to (GskPathBuilder *builder,
                          float           x,
                          float           y)
{
  g_return_if_fail (builder != NULL);

  gsk_path_builder_end_current (builder);

  builder->current_point = GRAPHENE_POINT_INIT(x, y);

  gsk_path_builder_ensure_current (builder);
}

/**
 * gsk_path_builder_rel_move_to:
 * @builder: a `GskPathBuilder`
 * @x: x offset
 * @y: y offset
 *
 * Starts a new contour by placing the pen at @x, @y relative to the current
 * point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.move_to].
 **/
void
gsk_path_builder_rel_move_to (GskPathBuilder *builder,
                              float           x,
                              float           y)
{
  g_return_if_fail (builder != NULL);

  gsk_path_builder_move_to (builder,
                            builder->current_point.x + x,
                            builder->current_point.y + y);
}

/**
 * gsk_path_builder_line_to:
 * @builder: a `GskPathBuilder`
 * @x: x coordinate
 * @y: y coordinate
 *
 * Draws a line from the current point to @x, @y and makes it the new current
 * point.
 **/
void
gsk_path_builder_line_to (GskPathBuilder *builder,
                          float           x,
                          float           y)
{
  g_return_if_fail (builder != NULL);

  /* skip the line if it goes to the same point */
  if (graphene_point_equal (&builder->current_point,
                            &GRAPHENE_POINT_INIT (x, y)))
    return;

  gsk_path_builder_append_current (builder,
                                   GSK_PATH_LINE,
                                   1, (graphene_point_t[1]) {
                                     GRAPHENE_POINT_INIT (x, y)
                                   });
}

/**
 * gsk_path_builder_rel_line_to:
 * @builder: a `GskPathBuilder`
 * @x: x offset
 * @y: y offset
 *
 * Draws a line from the current point to a point offset to it by @x, @y
 * and makes it the new current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.line_to].
 **/
void
gsk_path_builder_rel_line_to (GskPathBuilder *builder,
                              float           x,
                              float           y)
{
  g_return_if_fail (builder != NULL);

  gsk_path_builder_line_to (builder,
                            builder->current_point.x + x,
                            builder->current_point.y + y);
}

/**
 * gsk_path_builder_curve_to:
 * @builder: a `GskPathBuilder`
 * @x1: x coordinate of first control point
 * @y1: y coordinate of first control point
 * @x2: x coordinate of second control point
 * @y2: y coordinate of second control point
 * @x3: x coordinate of the end of the curve
 * @y3: y coordinate of the end of the curve
 *
 * Adds a [cubic Bézier curve](https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
 * from the current point to @x3, @y3 with @x1, @y1 and @x2, @y2 as the control
 * points.
 *
 * After this, @x3, @y3 will be the new current point.
 **/
void
gsk_path_builder_curve_to (GskPathBuilder *builder,
                           float           x1,
                           float           y1,
                           float           x2,
                           float           y2,
                           float           x3,
                           float           y3)
{
  g_return_if_fail (builder != NULL);

  builder->flags &= ~GSK_PATH_FLAT;
  gsk_path_builder_append_current (builder,
                                   GSK_PATH_CURVE,
                                   3, (graphene_point_t[3]) {
                                     GRAPHENE_POINT_INIT (x1, y1),
                                     GRAPHENE_POINT_INIT (x2, y2),
                                     GRAPHENE_POINT_INIT (x3, y3)
                                   });
}

/**
 * gsk_path_builder_rel_curve_to:
 * @builder: a `GskPathBuilder`
 * @x1: x offset of first control point
 * @y1: y offset of first control point
 * @x2: x offset of second control point
 * @y2: y offset of second control point
 * @x3: x offset of the end of the curve
 * @y3: y offset of the end of the curve
 *
 * Adds a [cubic Bézier curve](https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
 * from the current point to @x3, @y3 with @x1, @y1 and @x2, @y2 as the control
 * points. All coordinates are given relative to the current point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.curve_to].
 **/
void
gsk_path_builder_rel_curve_to (GskPathBuilder *builder,
                               float           x1,
                               float           y1,
                               float           x2,
                               float           y2,
                               float           x3,
                               float           y3)
{
  g_return_if_fail (builder != NULL);

  gsk_path_builder_curve_to (builder,
                             builder->current_point.x + x1,
                             builder->current_point.y + y1,
                             builder->current_point.x + x2,
                             builder->current_point.y + y2,
                             builder->current_point.x + x3,
                             builder->current_point.y + y3);
}

/**
 * gsk_path_builder_conic_to:
 * @builder: a `GskPathBuilder`
 * @x1: x coordinate of control point
 * @y1: y coordinate of control point
 * @x2: x coordinate of the end of the curve
 * @y2: y coordinate of the end of the curve
 * @weight: weight of the curve
 *
 * Adds a [conic curve](https://en.wikipedia.org/wiki/Non-uniform_rational_B-spline)
 * from the current point to @x2, @y2 with the given
 * @weight and @x1, @y1 as the single control point.
 *
 * Conic curves can be used to draw ellipses and circles.
 *
 * After this, @x2, @y2 will be the new current point.
 **/
void
gsk_path_builder_conic_to (GskPathBuilder *builder,
                           float           x1,
                           float           y1,
                           float           x2,
                           float           y2,
                           float           weight)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (weight >= 0);

  builder->flags &= ~GSK_PATH_FLAT;
  gsk_path_builder_append_current (builder,
                                   GSK_PATH_CONIC,
                                   3, (graphene_point_t[3]) {
                                     GRAPHENE_POINT_INIT (x1, y1),
                                     GRAPHENE_POINT_INIT (weight, 0),
                                     GRAPHENE_POINT_INIT (x2, y2)
                                   });
}

/**
 * gsk_path_builder_rel_conic_to:
 * @builder: a `GskPathBuilder`
 * @x1: x offset of control point
 * @y1: y offset of control point
 * @x2: x offset of the end of the curve
 * @y2: y offset of the end of the curve
 * @weight: weight of the curve
 *
 * Adds a [conic curve](https://en.wikipedia.org/wiki/Non-uniform_rational_B-spline)
 * from the current point to @x2, @y2 with the given
 * @weight and @x1, @y1 as the single control point.
 *
 * This is the relative version of [method@Gsk.PathBuilder.conic_to].
 **/
void
gsk_path_builder_rel_conic_to (GskPathBuilder *builder,
                               float           x1,
                               float           y1,
                               float           x2,
                               float           y2,
                               float           weight)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (weight >= 0);

  gsk_path_builder_conic_to (builder,
                             builder->current_point.x + x1,
                             builder->current_point.y + y1,
                             builder->current_point.x + x2,
                             builder->current_point.y + y2,
                             weight);
}

/**
 * gsk_path_builder_close:
 * @builder: a `GskPathBuilder`
 *
 * Ends the current contour with a line back to the start point.
 *
 * Note that this is different from calling [method@Gsk.PathBuilder.line_to]
 * with the start point in that the contour will be closed. A closed
 * contour behaves different from an open one when stroking its start
 * and end point are considered connected, so they will be joined
 * via the line join, and not ended with line caps.
 **/
void
gsk_path_builder_close (GskPathBuilder *builder)
{
  g_return_if_fail (builder != NULL);

  if (builder->ops->len == 0)
    return;

  builder->flags |= GSK_PATH_CLOSED;
  gsk_path_builder_append_current (builder,
                                   GSK_PATH_CLOSE,
                                   1, (graphene_point_t[1]) {
                                     g_array_index (builder->points, graphene_point_t, 0)
                                   });

  gsk_path_builder_end_current (builder);
}


static void
arc_segment (GskPathBuilder *builder,
             double          cx,
             double          cy,
             double          rx,
             double          ry,
             double          sin_phi,
             double          cos_phi,
             double          sin_th0,
             double          cos_th0,
             double          sin_th1,
             double          cos_th1,
             double          t)
{
  double x1, y1, x2, y2, x3, y3;

  x1 = rx * (cos_th0 - t * sin_th0);
  y1 = ry * (sin_th0 + t * cos_th0);
  x3 = rx * cos_th1;
  y3 = ry * sin_th1;
  x2 = x3 + rx * (t * sin_th1);
  y2 = y3 + ry * (-t * cos_th1);

  gsk_path_builder_curve_to (builder,
                             cx + cos_phi * x1 - sin_phi * y1,
                             cy + sin_phi * x1 + cos_phi * y1,
                             cx + cos_phi * x2 - sin_phi * y2,
                             cy + sin_phi * x2 + cos_phi * y2,
                             cx + cos_phi * x3 - sin_phi * y3,
                             cy + sin_phi * x3 + cos_phi * y3);
}

void
gsk_path_builder_svg_arc_to (GskPathBuilder *builder,
                             float           rx,
                             float           ry,
                             float           x_axis_rotation,
                             gboolean        large_arc,
                             gboolean        positive_sweep,
                             float           x,
                             float           y)
{
  graphene_point_t *current;
  double x1, y1, x2, y2;
  double phi, sin_phi, cos_phi;
  double mid_x, mid_y;
  double lambda;
  double d;
  double k;
  double x1_, y1_;
  double cx_, cy_;
  double  cx, cy;
  double ux, uy, u_len;
  double cos_theta1, theta1;
  double vx, vy, v_len;
  double dp_uv;
  double cos_delta_theta, delta_theta;
  int i, n_segs;
  double d_theta, theta;
  double sin_th0, cos_th0;
  double sin_th1, cos_th1;
  double th_half;
  double t;

  if (builder->points->len > 0)
    {
      current = &g_array_index (builder->points, graphene_point_t, builder->points->len - 1);
      x1 = current->x;
      y1 = current->y;
    }
  else
    {
      x1 = 0;
      y1 = 0;
    }
  x2 = x;
  y2 = y;

  phi = x_axis_rotation * M_PI / 180.0;
  sincos (phi, &sin_phi, &cos_phi);

  rx = fabs (rx);
  ry = fabs (ry);

  mid_x = (x1 - x2) / 2;
  mid_y = (y1 - y2) / 2;

  x1_ = cos_phi * mid_x + sin_phi * mid_y;
  y1_ = - sin_phi * mid_x + cos_phi * mid_y;

  lambda = (x1_ / rx) * (x1_ / rx) + (y1_ / ry) * (y1_ / ry);
  if (lambda > 1)
    {
      lambda = sqrt (lambda);
      rx *= lambda;
      ry *= lambda;
    }

  d = (rx * y1_) * (rx * y1_) + (ry * x1_) * (ry * x1_);
  if (d == 0)
    return;

  k = sqrt (fabs ((rx * ry) * (rx * ry) / d - 1.0));
  if (positive_sweep == large_arc)
    k = -k;

  cx_ = k * rx * y1_ / ry;
  cy_ = -k * ry * x1_ / rx;

  cx = cos_phi * cx_ - sin_phi * cy_ + (x1 + x2) / 2;
  cy = sin_phi * cx_ + cos_phi * cy_ + (y1 + y2) / 2;

  ux = (x1_ - cx_) / rx;
  uy = (y1_ - cy_) / ry;
  u_len = sqrt (ux * ux + uy * uy);
  if (u_len == 0)
    return;

  cos_theta1 = CLAMP (ux / u_len, -1, 1);
  theta1 = acos (cos_theta1);
  if (uy < 0)
    theta1 = - theta1;

  vx = (- x1_ - cx_) / rx;
  vy = (- y1_ - cy_) / ry;
  v_len = sqrt (vx * vx + vy * vy);
  if (v_len == 0)
    return;

  dp_uv = ux * vx + uy * vy;
  cos_delta_theta = CLAMP (dp_uv / (u_len * v_len), -1, 1);
  delta_theta = acos (cos_delta_theta);
  if (ux * vy - uy * vx < 0)
    delta_theta = - delta_theta;
  if (positive_sweep && delta_theta < 0)
    delta_theta += 2 * M_PI;
  else if (!positive_sweep && delta_theta > 0)
    delta_theta -= 2 * M_PI;

  n_segs = ceil (fabs (delta_theta / (M_PI_2 + 0.001)));
  d_theta = delta_theta / n_segs;
  theta = theta1;
  sincos (theta1, &sin_th1, &cos_th1);

  th_half = d_theta / 2;
  t = (8.0 / 3.0) * sin (th_half / 2) * sin (th_half / 2) / sin (th_half);

  for (i = 0; i < n_segs; i++)
    {
      theta = theta1;
      theta1 = theta + d_theta;
      sin_th0 = sin_th1;
      cos_th0 = cos_th1;
      sincos (theta1, &sin_th1, &cos_th1);
      arc_segment (builder,
                   cx, cy, rx, ry,
                   sin_phi, cos_phi,
                   sin_th0, cos_th0,
                   sin_th1, cos_th1,
                   t);
    }
}

/**
 * gsk_path_builder_add_layout:
 * @builder: a `GskPathBuilder`
 * @layout: the pango layout to add
 *
 * Adds the outlines for the glyphs in @layout to @builder.
 */
void
gsk_path_builder_add_layout (GskPathBuilder *builder,
                             PangoLayout    *layout)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_path_t *cairo_path;
  GskPath *path;

  surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, NULL);
  cr = cairo_create (surface);

  pango_cairo_layout_path (cr, layout);
  cairo_path = cairo_copy_path_flat (cr);
  path = gsk_path_new_from_cairo (cairo_path);

  gsk_path_builder_add_path (builder, path);

  gsk_path_unref (path);

  cairo_path_destroy (cairo_path);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);
}

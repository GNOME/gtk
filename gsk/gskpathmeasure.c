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

#include "gskpathmeasure.h"

#include "gskpathbuilder.h"
#include "gskpathprivate.h"

/**
 * GskPathMeasure:
 *
 * `GskPathMeasure` is an object that allows measuring operations
 * on `GskPath` objects, to determine quantities like the arc
 * length of the path, its curvature, or closest points.
 *
 * These operations are useful when implementing animations.
 */

typedef struct _GskContourMeasure GskContourMeasure;

struct _GskContourMeasure
{
  float length;
  gpointer contour_data;
};

struct _GskPathMeasure
{
  /*< private >*/
  guint ref_count;

  GskPath *path;
  float tolerance;

  gsize first;
  gsize last;

  float length;
  gsize n_contours;
  GskContourMeasure measures[];
};

G_DEFINE_BOXED_TYPE (GskPathMeasure, gsk_path_measure,
                     gsk_path_measure_ref,
                     gsk_path_measure_unref)

/**
 * gsk_path_measure_new:
 * @path: the path to measure
 *
 * Creates a measure object for the given @path with a
 * default tolerance.
 *
 * Returns: a new `GskPathMeasure` representing @path
 **/
GskPathMeasure *
gsk_path_measure_new (GskPath *path)
{
  return gsk_path_measure_new_with_tolerance (path, GSK_PATH_TOLERANCE_DEFAULT);
}

/**
 * gsk_path_measure_new_with_tolerance:
 * @path: the path to measure
 * @tolerance: the tolerance for measuring operations
 *
 * Creates a measure object for the given @path and @tolerance.
 *
 * Returns: a new `GskPathMeasure` representing @path
 **/
GskPathMeasure *
gsk_path_measure_new_with_tolerance (GskPath *path,
                                     float    tolerance)
{
  GskPathMeasure *self;
  gsize i, n_contours;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (tolerance > 0, NULL);

  n_contours = gsk_path_get_n_contours (path);

  self = g_malloc0 (sizeof (GskPathMeasure) + n_contours * sizeof (GskContourMeasure));

  self->ref_count = 1;
  self->path = gsk_path_ref (path);
  self->tolerance = tolerance;
  self->n_contours = n_contours;
  self->first = 0;
  self->last = n_contours;

  for (i = 0; i < n_contours; i++)
    {
      self->measures[i].contour_data = gsk_contour_init_measure (gsk_path_get_contour (path, i),
                                                                 self->tolerance,
                                                                 &self->measures[i].length);
      self->length += self->measures[i].length;
    }

  return self;
}

/**
 * gsk_path_measure_ref:
 * @self: a `GskPathMeasure`
 *
 * Increases the reference count of a `GskPathMeasure` by one.
 *
 * Returns: the passed in `GskPathMeasure`.
 **/
GskPathMeasure *
gsk_path_measure_ref (GskPathMeasure *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count++;

  return self;
}

/**
 * gsk_path_measure_unref:
 * @self: a `GskPathMeasure`
 *
 * Decreases the reference count of a `GskPathMeasure` by one.
 *
 * If the resulting reference count is zero, frees the object.
 **/
void
gsk_path_measure_unref (GskPathMeasure *self)
{
  gsize i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  self->ref_count--;
  if (self->ref_count > 0)
    return;

  for (i = 0; i < self->n_contours; i++)
    {
      gsk_contour_free_measure (gsk_path_get_contour (self->path, i),
                                self->measures[i].contour_data);
    }

  gsk_path_unref (self->path);
  g_free (self);
}

/**
 * gsk_path_measure_get_path:
 * @self: a `GskPathMeasure`
 *
 * Returns the path that the measure was created for.
 *
 * Returns: (transfer none): the path of @self
 */
GskPath *
gsk_path_measure_get_path (GskPathMeasure *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->path;
}

/**
 * gsk_path_measure_get_tolerance:
 * @self: a `GskPathMeasure`
 *
 * Returns the tolerance that the measure was created with.
 *
 * Returns: the tolerance of @self
 */
float
gsk_path_measure_get_tolerance (GskPathMeasure *self)
{
  g_return_val_if_fail (self != NULL, 0.f);

  return self->tolerance;
}

/**
 * gsk_path_measure_get_n_contours:
 * @self: a `GskPathMeasure`
 *
 * Returns the number of contours in the path being measured.
 *
 * The returned value is independent of whether @self if restricted
 * or not.
 *
 * Returns: The number of contours
 **/
gsize
gsk_path_measure_get_n_contours (GskPathMeasure *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->n_contours;
}

/**
 * gsk_path_measure_restrict_to_contour:
 * @self: a `GskPathMeasure`
 * @contour: contour to restrict to or (gsize) -1 for using the
 *   whole path
 *
 * Restricts all functions on the path to just the given @contour.
 *
 * If @contour >= gsk_path_measure_get_n_contours() - so in
 * particular when it is set to -1 - the whole path will be used.
 **/
void
gsk_path_measure_restrict_to_contour (GskPathMeasure *self,
                                      gsize           contour)
{
  if (contour >= self->n_contours)
    {
      /* use the whole path */
      self->first = 0;
      self->last = self->n_contours;
    }
  else
    {
      /* use just one contour */
      self->first = contour;
      self->last = contour + 1;
    }

  self->length = 0;
  for (gsize i = self->first; i < self->last; i++)
    {
      self->length += self->measures[i].length;
    }
}

/**
 * gsk_path_measure_get_length:
 * @self: a `GskPathMeasure`
 *
 * Gets the length of the path being measured.
 *
 * The length is cached, so this function does not do any work.
 *
 * Returns: The length of the path measured by @self
 **/
float
gsk_path_measure_get_length (GskPathMeasure *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->length;
}

/**
 * gsk_path_measure_is_closed:
 * @self: a `GskPathMeasure`
 *
 * Returns if the path being measured represents a single closed
 * contour.
 *
 * Returns: %TRUE if the current path is closed
 **/
gboolean
gsk_path_measure_is_closed (GskPathMeasure *self)
{
  const GskContour *contour;

  g_return_val_if_fail (self != NULL, FALSE);

  /* XXX: is the empty path closed? Currently it's not */
  if (self->last - self->first != 1)
    return FALSE;

  contour = gsk_path_get_contour (self->path, self->first);
  return gsk_contour_get_flags (contour) & GSK_PATH_CLOSED ? TRUE : FALSE;
}

static float
gsk_path_measure_clamp_distance (GskPathMeasure *self,
                                 float           distance)
{
  if (isnan (distance))
    return 0;

  return CLAMP (distance, 0, self->length);
}

/**
 * gsk_path_measure_get_point:
 * @self: a `GskPathMeasure`
 * @distance: distance into the path
 * @pos: (optional) (out caller-allocates): The coordinates
 *   of the position at @distance
 * @tangent: (optional) (out caller-allocates): The tangent
 *   to the position at @distance
 *
 * Calculates the coordinates and tangent of the point @distance
 * units into the path. The value will be clamped to the length
 * of the path.
 *
 * If the point is a discontinuous edge in the path, the returned
 * point and tangent will describe the line starting at that point
 * going forward.
 *
 * If @self describes an empty path, the returned point will be
 * set to `(0, 0)` and the tangent will be the x axis or `(1, 0)`.
 **/
void
gsk_path_measure_get_point (GskPathMeasure   *self,
                            float             distance,
                            graphene_point_t *pos,
                            graphene_vec2_t  *tangent)
{
  gsize i;

  g_return_if_fail (self != NULL);

  if (pos == NULL && tangent == NULL)
    return;

  distance = gsk_path_measure_clamp_distance (self, distance);

  for (i = self->first; i < self->last; i++)
    {
      if (distance < self->measures[i].length)
        break;

      distance -= self->measures[i].length;
    }

  /* weird corner cases */
  if (i == self->last)
    {
      /* the empty path goes here */
      if (self->first == self->last)
        {
          if (pos)
            graphene_point_init (pos, 0.f, 0.f);
          if (tangent)
            graphene_vec2_init (tangent, 1.f, 0.f);
          return;
        }
      /* rounding errors can make this happen */
      i = self->last - 1;
      distance = self->measures[i].length;
    }

  gsk_contour_get_point (gsk_path_get_contour (self->path, i),
                         self->measures[i].contour_data,
                         distance,
                         pos,
                         tangent);
}

/**
 * gsk_path_measure_get_closest_point:
 * @self: a `GskPathMeasure`
 * @point: the point to find the closest point to
 * @out_pos: (optional) (out caller-allocates): return location
 *   for the closest point
 *
 * Gets the point on the path that is closest to @point.
 *
 * If the path being measured is empty, return 0 and set
 * @out_pos to (0, 0).
 *
 * This is a simpler and slower version of
 * [method@Gsk.PathMeasure.get_closest_point_full].
 * Use that one if you need more control.
 *
 * Returns: The offset into the path of the closest point
 **/
float
gsk_path_measure_get_closest_point (GskPathMeasure         *self,
                                    const graphene_point_t *point,
                                    graphene_point_t       *out_pos)
{
  float result;

  g_return_val_if_fail (self != NULL, 0.0f);

  if (gsk_path_measure_get_closest_point_full (self,
                                               point,
                                               INFINITY,
                                               NULL,
                                               out_pos,
                                               &result,
                                               NULL))
    return result;

  if (out_pos)
    *out_pos = GRAPHENE_POINT_INIT (0, 0);

  return 0;

}

/**
 * gsk_path_measure_get_closest_point_full:
 * @self: a `GskPathMeasure`
 * @point: the point to find the closest point to
 * @threshold: The maximum allowed distance between the path and @point.
 *   Use INFINITY to look for any point.
 * @out_distance: (optional) (out caller-allocates): The
 *   distance between the found closest point on the path and the given
 *   @point.
 * @out_pos: (optional) (out caller-allocates): return location
 *   for the closest point
 * @out_offset: (optional) (out caller-allocates): The offset into
 *   the path of the found point
 * @out_tangent: (optional) (out caller-allocates): return location for
 *   the tangent at the closest point
 *
 * Gets the point on the path that is closest to @point. If no point on
 * path is closer to @point than @threshold, return %FALSE.
 *
 * Returns: %TRUE if a point was found, %FALSE otherwise.
 **/
gboolean
gsk_path_measure_get_closest_point_full (GskPathMeasure         *self,
                                         const graphene_point_t *point,
                                         float                   threshold,
                                         float                  *out_distance,
                                         graphene_point_t       *out_pos,
                                         float                  *out_offset,
                                         graphene_vec2_t        *out_tangent)
{
  gboolean result;
  gsize i;
  float distance, length;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (point != NULL, FALSE);

  result = FALSE;
  length = 0;

  for (i = self->first; i < self->last; i++)
    {
      if (gsk_contour_get_closest_point (gsk_path_get_contour (self->path, i),
                                         self->measures[i].contour_data,
                                         self->tolerance,
                                         point,
                                         threshold,
                                         &distance,
                                         out_pos,
                                         out_offset,
                                         out_tangent))
        {
          result = TRUE;
          if (out_offset)
            *out_offset += length;

          if (distance < self->tolerance)
            break;
          threshold = distance - self->tolerance;
        }

      length += self->measures[i].length;
    }

  if (result && out_distance)
    *out_distance = distance;

  return result;
}

/**
 * gsk_path_measure_in_fill:
 * @self: a #GskPathMeasure
 * @point: the point to test
 * @fill_rule: the fill rule to follow
 *
 * Returns whether the given point is inside the area that would be
 * affected if the path of @self was filled according to @fill_rule.
 *
 * Returns: %TRUE if @point is inside
 */
gboolean
gsk_path_measure_in_fill (GskPathMeasure   *self,
                          graphene_point_t *point,
                          GskFillRule       fill_rule)
{
  int winding = 0;
  gboolean on_edge = FALSE;
  int i;

  for (i = self->first; i < self->last; i++)
    {
      winding += gsk_contour_get_winding (gsk_path_get_contour (self->path, i),
                                          self->measures[i].contour_data,
                                          point,
                                          &on_edge);
      if (on_edge)
        return TRUE;
    }

  switch (fill_rule)
    {
    case GSK_FILL_RULE_EVEN_ODD:
      return winding & 1;
    case GSK_FILL_RULE_WINDING:
      return winding != 0;
    default:
      g_assert_not_reached ();
    }
}

static void
gsk_path_builder_add_segment_chunk (GskPathBuilder *self,
                                    GskPathMeasure *measure,
                                    gboolean        emit_move_to,
                                    float           start,
                                    float           end)
{
  g_assert (start < end);

  for (gsize i = measure->first; i < measure->last; i++)
    {
      if (measure->measures[i].length < start)
        {
          start -= measure->measures[i].length;
          end -= measure->measures[i].length;
        }
      else if (start > 0 || end < measure->measures[i].length)
        {
          float len = MIN (end, measure->measures[i].length);
          gsk_contour_add_segment (gsk_path_get_contour (measure->path, i),
                                   self,
                                   measure->measures[i].contour_data,
                                   emit_move_to,
                                   start,
                                   len);
          end -= len;
          start = 0;
          if (end <= 0)
            break;
        }
      else
        {
          end -= measure->measures[i].length;
          gsk_path_builder_add_contour (self, gsk_contour_dup (gsk_path_get_contour (measure->path, i)));
        }
      emit_move_to = TRUE;
    }
}

/**
 * gsk_path_builder_add_segment:
 * @builder: a `GskPathBuilder`
 * @measure: the `GskPathMeasure` to take the segment to
 * @start: start distance into the path
 * @end: end distance into the path
 *
 * Adds to @builder the segment of @measure from @start to @end.
 *
 * The distances are given relative to the length of @measure's path,
 * from 0 for the beginning of the path to [method@Gsk.PathMeasure.get_length]
 * for the end of the path. The values will be clamped to that range.
 *
 * If @start >= @end after clamping, the path will first add the segment
 * from @start to the end of the path, and then add the segment from
 * the beginning to @end. If the path is closed, these segments will
 * be connected.
 **/
void
gsk_path_builder_add_segment (GskPathBuilder *builder,
                              GskPathMeasure *measure,
                              float           start,
                              float           end)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (measure != NULL);

  start = gsk_path_measure_clamp_distance (measure, start);
  end = gsk_path_measure_clamp_distance (measure, end);

  if (start < end)
    {
      gsk_path_builder_add_segment_chunk (builder, measure, TRUE, start, end);
    }
  else
    {
      /* If the path is closed, we can connect the 2 subpaths. */
      gboolean closed = gsk_path_measure_is_closed (measure);
      gboolean need_move_to = !closed;

      if (start < measure->length)
        gsk_path_builder_add_segment_chunk (builder, measure,
                                            TRUE,
                                            start, measure->length);
      else
        need_move_to = TRUE;

      if (end > 0)
        gsk_path_builder_add_segment_chunk (builder, measure,
                                            need_move_to,
                                            0, end);
      if (start == end && closed)
        gsk_path_builder_close (builder);
    }
}

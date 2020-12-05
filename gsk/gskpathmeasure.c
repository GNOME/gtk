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

#include "gskpathbuilder.h"
#include "gskpathpointprivate.h"
#include "gskpathprivate.h"

/**
 * `GskPathMeasure` is an object that allows measurements
 * on `GskPath`s such as determining the length of the path.
 *
 * Many measuring operations require approximating the path
 * with simpler shapes. Therefore, a `GskPathMeasure` has
 * a tolerance that determines what amount is required
 * for such approximations.
 *
 * A `GskPathMeasure` struct is a reference counted struct
 * and should be treated as opaque.
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
 * Creates a measure object for the given @path.
 *
 * Returns: a new `GskPathMeasure` representing @path
 */
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
 */
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
 */
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
 */
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
  return self->tolerance;
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
 */
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
 * Returns: `TRUE` if the current path is closed
 */
gboolean
gsk_path_measure_is_closed (GskPathMeasure *self)
{
  const GskContour *contour;

  g_return_val_if_fail (self != NULL, FALSE);

  /* XXX: is the empty path closed? Currently it's not */
  if (self->n_contours != 1)
    return FALSE;

  contour = gsk_path_get_contour (self->path, 0);
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
 * gsk_path_measure_in_fill:
 * @self: a `GskPathMeasure`
 * @point: the point to test
 * @fill_rule: the fill rule to follow
 *
 * Returns whether the given point is inside the area
 * that would be affected if the path was filled according
 * to @fill_rule.
 *
 * Returns: `TRUE` if @point is inside
 */
gboolean
gsk_path_measure_in_fill (GskPathMeasure         *self,
                          const graphene_point_t *point,
                          GskFillRule             fill_rule)
{
  int winding = 0;
  int i;

  for (i = 0; i < self->n_contours; i++)
    winding += gsk_contour_get_winding (gsk_path_get_contour (self->path, i),
                                        self->measures[i].contour_data,
                                        point);

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

  for (gsize i = 0; i < measure->n_contours; i++)
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
 * @self: a `GskPathBuilder`
 * @measure: the `GskPathMeasure` to take the segment to
 * @start: start distance into the path
 * @end: end distance into the path
 *
 * Adds to @self the segment of @measure from @start to @end.
 *
 * The distances are given relative to the length of @measure's path,
 * from 0 for the beginning of the path to its length for the end
 * of the path. The values will be clamped to that range. The length
 * can be obtained with [method@Gsk.PathMeasure.get_length].
 *
 * If @start >= @end after clamping, the path will first add the segment
 * from @start to the end of the path, and then add the segment from
 * the beginning to @end. If the path is closed, these segments will
 * be connected.
 */
void
gsk_path_builder_add_segment (GskPathBuilder *self,
                              GskPathMeasure *measure,
                              float           start,
                              float           end)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (measure != NULL);

  start = gsk_path_measure_clamp_distance (measure, start);
  end = gsk_path_measure_clamp_distance (measure, end);

  if (start < end)
    {
      gsk_path_builder_add_segment_chunk (self, measure, TRUE, start, end);
    }
  else
    {
      /* If the path is closed, we can connect the 2 subpaths. */
      gboolean closed = gsk_path_measure_is_closed (measure);
      gboolean need_move_to = !closed;

      if (start < measure->length)
        gsk_path_builder_add_segment_chunk (self, measure,
                                            TRUE,
                                            start, measure->length);
      else
        need_move_to = TRUE;

      if (end > 0)
        gsk_path_builder_add_segment_chunk (self, measure,
                                            need_move_to,
                                            0, end);
      if (start == end && closed)
        gsk_path_builder_close (self);
    }
}

/**
 * gsk_path_measure_get_point:
 * @self: a `GskPathMeasure`
 * @distance: the distance
 *
 * Returns a `GskPathPoint` representing the point at the given
 * distance into the path.
 *
 * An empty path has no points, so `NULL` is returned in that case.
 *
 * Returns: (transfer full) (nullable): a newly allocated `GskPathPoint`
 */
GskPathPoint *
gsk_path_measure_get_point (GskPathMeasure *self,
                            float           distance)
{
  gsize i;
  float contour_offset;
  float offset;
  const GskContour *contour;

  g_return_val_if_fail (self != NULL, NULL);

  if (self->n_contours == 0)
    return NULL;

  contour_offset = 0;
  offset = gsk_path_measure_clamp_distance (self, distance);

  for (i = 0; i < self->n_contours - 1; i++)
    {
      if (offset < self->measures[i].length)
        break;

      contour_offset += self->measures[i].length;
      offset -= self->measures[i].length;
    }

  g_assert (0 <= i && i < self->n_contours);

  offset = CLAMP (offset, 0, self->measures[i].length);

  contour = gsk_path_get_contour (self->path, i);

  return gsk_path_point_new (self,
                             contour, self->measures[i].contour_data,
                             contour_offset, offset);
}

/**
 * gsk_path_measure_get_closest_point:
 * @self: a `GskPathMeasure`
 * @point: the point to fond the closest point to
 * @threshold: The maximum allowed distance between the path and @point.
 *   Use `INFINITY` to look for any point.
 *
 * Returns a `GskPathPoint` representing the point on the path
 * that is closest to the given point.
 *
 * If no point on the path is closer than @threshold, `NULL` is returned.
 *
 * Returns: (transfer full) (nullable): a newly allocated `GskPathPoint`
 */
GskPathPoint *
gsk_path_measure_get_closest_point (GskPathMeasure         *self,
                                    const graphene_point_t *point,
                                    float                   threshold)
{
  gssize best_idx;
  float best_offset;
  float best_contour_offset;
  float contour_offset;

  contour_offset = 0;
  best_idx = -1;

  for (gsize i = 0; i < self->n_contours; i++)
    {
      float distance, offset;

      if (gsk_contour_get_closest_point (gsk_path_get_contour (self->path, i),
                                         self->measures[i].contour_data,
                                         self->tolerance,
                                         point,
                                         threshold,
                                         &distance,
                                         NULL,
                                         &offset,
                                         NULL))
        {
          best_idx = i;
          best_offset = offset;
          best_contour_offset = contour_offset;

          if (distance < self->tolerance)
            break;

          threshold = distance - self->tolerance;
        }

      contour_offset += self->measures[i].length;
    }

  if (best_idx != -1)
    return gsk_path_point_new (self,
                               gsk_path_get_contour (self->path, best_idx),
                               self->measures[best_idx].contour_data,
                               best_contour_offset, best_offset);

  return NULL;
}

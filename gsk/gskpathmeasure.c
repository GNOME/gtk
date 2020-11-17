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

  for (i = 0; i < n_contours; i++)
    {
      self->measures[i].contour_data = gsk_contour_init_measure (path, i,
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
      gsk_contour_free_measure (self->path, i, self->measures[i].contour_data);
    }

  gsk_path_unref (self->path);
  g_free (self);
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
 * gsk_path_measure_add_segment:
 * @self: a `GskPathMeasure`
 * @builder: the builder to add the segment to
 * @start: start distance into the path
 * @end: end distance into the path
 *
 * Adds to @builder the segment of @path inbetween @start and @end.
 *
 * The distances are given relative to the length of @self's path,
 * from 0 for the beginning of the path to [method@Gsk.PathMeasure.get_length]
 * for the end of the path. The values will be clamped to that range.
 *
 * If @start >= @end after clamping, no path will be added.
 **/
void
gsk_path_measure_add_segment (GskPathMeasure *self,
                              GskPathBuilder *builder,
                              float           start,
                              float           end)
{
  gsize i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (builder != NULL);

  start = CLAMP (start, 0, self->length);
  end = CLAMP (end, 0, self->length);
  if (start >= end)
    return;

  for (i = 0; i < self->n_contours; i++)
    {
      if (self->measures[i].length < start)
        {
          start -= self->measures[i].length;
          end -= self->measures[i].length;
        }
      else if (start > 0 || end < self->measures[i].length)
        {
          float len = MIN (end, self->measures[i].length);
          gsk_path_builder_add_contour_segment (builder,
                                                self->path,
                                                i,
                                                self->measures[i].contour_data,
                                                start,
                                                len);
          start = 0;
          end -= len;
        }
      else
        {
          end -= self->measures[i].length;
          gsk_path_builder_add_contour (builder, self->path, i);
        }
    }
}

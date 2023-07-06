#include "config.h"

#include "gskpathpointprivate.h"

#include "gskcontourprivate.h"
#include "gskpathmeasure.h"

#include "gdk/gdkprivate.h"


/**
 * GskPathPoint:
 *
 * `GskPathPoint` is an opaque, immutable type
 * representing a point on a path.
 *
 * It can be queried for properties of the path
 * at that point, such as its tangent or its curvature.
 *
 * To obtain a `GskPathPoint`, use
 * [method@Gsk.PathMeasure.get_path_point] or
 * [method@Gsk.PathMeasure.get_closest_point].
 */

struct _GskPathPoint
{
  guint ref_count;

  GskPathMeasure *measure;
  const GskContour *contour;
  gpointer measure_data;
  float contour_offset;  /* distance from beginning of path to contour */
  float offset;          /* offset of point inside contour */
};

G_DEFINE_BOXED_TYPE (GskPathPoint, gsk_path_point,
                     gsk_path_point_ref,
                     gsk_path_point_unref)

GskPathPoint *
gsk_path_point_new (GskPathMeasure   *measure,
                    const GskContour *contour,
                    gpointer          measure_data,
                    float             contour_offset,
                    float             offset)
{
  GskPathPoint *self;

  self = g_new0 (GskPathPoint, 1);

  self->ref_count = 1;

  self->measure = gsk_path_measure_ref (measure);
  self->contour = contour;
  self->measure_data = measure_data;
  self->contour_offset = contour_offset;
  self->offset = offset;

  return self;
}

/**
 * gsk_path_point_ref:
 * @self: a #GskPathPoint
 *
 * Increases the reference count of a #GskPathPoint by one.
 *
 * Returns: the passed in #GskPathPoint
 */
GskPathPoint *
gsk_path_point_ref (GskPathPoint *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count++;

  return self;
}

/**
 * gsk_path_point_unref:
 * @self: a #GskPathPoint
 *
 * Decreases the reference count of a #GskPathPoint by one.
 *
 * If the resulting reference count is zero, frees the path_measure.
 */
void
gsk_path_point_unref (GskPathPoint *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  self->ref_count--;
  if (self->ref_count > 0)
    return;

  gsk_path_measure_unref (self->measure);

  g_free (self);
}

GskPathMeasure *
gsk_path_point_get_measure (GskPathPoint *self)
{
  return self->measure;
}

/**
 * gsk_path_point_get_offset:
 * @self: a `GskPathPoint`
 *
 * Returns the offset of the given point
 * into the path.
 *
 * This is the length of the contour from
 * the beginning of the path to the point.
 *
 * Returns: The offset of point in path
 */
float
gsk_path_point_get_offset (GskPathPoint *self)
{
  return self->contour_offset + self->offset;
}

/**
 * gsk_path_point_get_point:
 * @self: a `GskPathPoint`
 * @point: (out caller-allocates): Return location for
 *   the coordinates of the point
 *
 * Gets the coordinates of the point.
 */
void
gsk_path_point_get_point (GskPathPoint     *self,
                          graphene_point_t *point)
{
  gsk_contour_get_point (self->contour, self->measure_data, self->offset, point, NULL);
}

/**
 * gsk_path_point_get_tangent:
 * @self: a `GskPathPoint`
 * @tangent: (out caller-allocates): Return location for
 *   the tangent at the point
 *
 * Gets the tangent of the path at the point.
 *
 * Note that certain special points on a path may
 * not have a single tangent, e.g. sharp turns, or cusps.
 * At such points, there are two tangents (the direction
 * of the path going into the point, and the direction
 * coming out of it. FIXME: add api to handle this.
 */
void
gsk_path_point_get_tangent (GskPathPoint    *self,
                            graphene_vec2_t *tangent)
{
  gsk_contour_get_point (self->contour, self->measure_data, self->offset, NULL, tangent);
}

/**
 * gsk_path_point_get_curvature:
 * @self: a `GskPathPoint`
 * @center: (out caller-allocates) (optional): Return location
 *   for the center of the osculating circle
 *
 * Calculates the curvature at the point.
 *
 * Optionally, also returns the center of the osculating
 * circle.
 *
 * If the curvature is infinite (at line segments), or does
 * not exist (at sharp turns), zero is returned, and @center
 * is not modified.
 *
 * Returns: The curvature of the path at the given point
 */
float
gsk_path_point_get_curvature (GskPathPoint     *self,
                              graphene_point_t *center)
{
  return gsk_contour_get_curvature (self->contour, self->measure_data, self->offset, center);
}

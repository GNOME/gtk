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

#include "gskpathprivate.h"

#include "gskcurveprivate.h"
#include "gskpathbuilder.h"
#include "gskpathpoint.h"
#include "gskcontourprivate.h"

#undef DEBUG

typedef struct
{
  GskPathPoint point1;
  GskPathPoint point2;
  GskPathIntersection kind;
} Intersection;

typedef struct
{
  GskPath *path1;
  GskPath *path2;
  GskPathIntersectionFunc func;
  gpointer data;

  gsize contour1;
  gsize contour2;
  gsize idx1;
  gsize idx2;

  const GskContour *c1;
  const GskContour *c2;
  GskCurve curve1;
  GskCurve curve2;

  gboolean c1_closed;
  gboolean c2_closed;
  gboolean c1_z_is_empty;
  gboolean c2_z_is_empty;

  gsize c1_count;
  gsize c2_count;

  GArray *points;
  GArray *all_points;
} PathIntersectData;

/* {{{ Utilities */

typedef struct
{
  gsize count;
  gboolean closed;
  gboolean z_is_empty;
} CountCurveData;

static gboolean
count_cb (GskPathOperation        op,
          const graphene_point_t *pts,
          gsize                   n_pts,
          float                   weight,
          gpointer                data)
{
  CountCurveData *ccd = data;

  (ccd->count)++;

  if (op ==GSK_PATH_CLOSE)
    {
      ccd->closed = TRUE;
      ccd->z_is_empty = graphene_point_equal (&pts[0], &pts[1]);
    }

  return TRUE;
}

static gsize
count_curves (const GskContour *contour,
              gboolean         *closed,
              gboolean         *z_is_empty)
{
  CountCurveData data;

  data.count = 0;
  data.closed = FALSE;
  data.z_is_empty = FALSE;

  gsk_contour_foreach (contour, count_cb, &data);

  *closed = data.closed;
  *z_is_empty = data.z_is_empty;

  return data.count;
}

/* }}} */
/* {{{ Intersecting general contours */

static gboolean
gsk_path_point_near (const GskPathPoint *p1,
                     const GskPathPoint *p2,
                     gboolean            closed,
                     gsize               count,
                     gboolean            z_is_empty,
                     float               epsilon)
{
  if (p1->idx == p2->idx && fabsf (p1->t - p2->t) < epsilon)
    return TRUE;

  if (p1->idx + 1 == p2->idx && (1 - p1->t + p2->t < epsilon))
    return TRUE;

  if (p2->idx + 1 == p1->idx && (1 - p2->t + p1->t < epsilon))
    return TRUE;

  if (closed)
    {
      if (p1->idx == 1 && p2->idx == count - 1 && (1 - p2->t + p1->t < epsilon))
        return TRUE;

      if (p2->idx == 1 && p1->idx == count - 1 && (1 - p1->t + p2->t < epsilon))
        return TRUE;
    }

  if (closed && z_is_empty)
    {
      if (p1->idx == 1 && p2->idx == count - 2 && (1 - p2->t + p1->t < epsilon))
        return TRUE;

      if (p2->idx == 1 && p1->idx == count - 2 && (1 - p1->t + p2->t < epsilon))
        return TRUE;
    }

  return FALSE;
}

static gboolean
intersect_curve2 (GskPathOperation        op,
                  const graphene_point_t *pts,
                  gsize                   n_pts,
                  float                   weight,
                  gpointer                data)
{
  PathIntersectData *pd = data;
  float t1[10], t2[10];
  graphene_point_t p[10];
  GskPathIntersection kind[10];
  int n;

  if (op == GSK_PATH_MOVE)
    {
      if (gsk_contour_get_n_ops (pd->c2) == 1)
        {
          float dist, tt;

          if (gsk_curve_get_closest_point (&pd->curve1, &pts[0], 1, &dist, &tt) && dist == 0)
            {
              Intersection is;

              is.kind = GSK_PATH_INTERSECTION_NORMAL;
              is.point1.contour = pd->contour1;
              is.point1.idx = pd->idx1;
              is.point1.t = tt;
              is.point2.contour = pd->contour2;
              is.point2.idx = 0;
              is.point2.t = 1;

              g_array_append_val (pd->points, is);
            }
        }

      return TRUE;
    }

  if (op == GSK_PATH_CLOSE)
    {
      if (graphene_point_equal (&pts[0], &pts[1]))
        return TRUE;
    }

  pd->idx2++;

  gsk_curve_init_foreach (&pd->curve2, op, pts, n_pts, weight);

#ifdef DEBUG
  {
    char *s1 = gsk_curve_to_string (&pd->curve1);
    char *s2 = gsk_curve_to_string (&pd->curve2);
    g_print ("intersecting %s and %s\n", s1, s2);
    g_free (s2);
    g_free (s1);
  }
#endif

  /* Cubic curves may have self-intersections */
  if (pd->path1 == pd->path2 &&
      pd->contour1 == pd->contour2 &&
      pd->idx1 == pd->idx2)
    {
      n = gsk_curve_self_intersect (&pd->curve1, t1, p, 10);
      for (int i = 0; i < n; i++)
        kind[i] = GSK_PATH_INTERSECTION_NORMAL;
    }
  else
    n = gsk_curve_intersect (&pd->curve1, &pd->curve2, t1, t2, p, kind, 10);

  for (int i = 0; i < n; i++)
    {
      Intersection is;

      is.point1.contour = pd->contour1;
      is.point2.contour = pd->contour2;
      is.point1.idx = pd->idx1;
      is.point2.idx = pd->idx2;
      is.point1.t = t1[i];
      is.point2.t = t2[i];
      is.kind = kind[i];

      /* Skip the shared point between two adjacent curves,
       * when we're looking for self-intersections
       */
      if (is.kind == GSK_PATH_INTERSECTION_NORMAL &&
          pd->path1 == pd->path2 &&
          pd->contour1 == pd->contour2 &&
          pd->idx1 != pd->idx2 &&
          gsk_path_point_near (&is.point1, &is.point2,
                               pd->c1_closed, pd->c1_count, pd->c1_z_is_empty,
                               0.001))
        {
          is.kind = GSK_PATH_INTERSECTION_NONE;
        }

#ifdef DEBUG
      {
        const char *kn[] = { "none", "normal", "start", "end" };
        g_print ("append p1 { %lu %lu %f } p2 { %lu %lu %f } %s\n",
                 is.point1.contour, is.point1.idx, is.point1.t,
                 is.point2.contour, is.point2.idx, is.point2.t,
                 kn[is.kind]);
      }
#endif
      g_array_append_val (pd->points, is);
    }

  return TRUE;
}

static gboolean
intersect_curve (GskPathOperation        op,
                 const graphene_point_t *pts,
                 gsize                   n_pts,
                 float                   weight,
                 gpointer                data)
{
  PathIntersectData *pd = data;
  GskBoundingBox b1, b2;

  if (op == GSK_PATH_MOVE)
    {
      if (gsk_contour_get_n_ops (pd->c1) == 1)
        {
          GskPathPoint point;
          float dist;

          if (gsk_contour_get_closest_point (pd->c2, &pts[0], 1, &point, &dist) && dist == 0)
            {
              Intersection is;

              is.kind = GSK_PATH_INTERSECTION_NORMAL;
              is.point1.contour = pd->contour1;
              is.point1.idx = 0;
              is.point1.t = 1;
              is.point2.contour = pd->contour2;
              is.point2.idx = point.idx;
              is.point2.t = point.t;

              g_array_append_val (pd->points, is);
            }
        }

      return TRUE;
    }

  if (op == GSK_PATH_CLOSE)
    {
      if (graphene_point_equal (&pts[0], &pts[1]))
        return TRUE;
    }

  pd->idx1++;

  gsk_curve_init_foreach (&pd->curve1, op, pts, n_pts, weight);
  gsk_curve_get_bounds (&pd->curve1, &b1);

  gsk_contour_get_bounds (pd->c2, &b2);

  if (gsk_bounding_box_intersection (&b1, &b2, NULL))
    {
      pd->idx2 = 0;

      if (!gsk_contour_foreach (pd->c2, intersect_curve2, pd))
        return FALSE;
    }

  return TRUE;
}

static int cmp_path1 (gconstpointer p1, gconstpointer p2);

static void
default_contour_collect_intersections (const GskContour  *contour1,
                                       const GskContour  *contour2,
                                       PathIntersectData *pd)
{
  pd->idx1 = 0;

  g_array_set_size (pd->points, 0);

  gsk_contour_foreach (contour1, intersect_curve, pd);

  g_array_sort (pd->points, cmp_path1);

#ifdef DEBUG
  g_print ("after sorting\n");
  for (gsize i = 0; i < pd->points->len; i++)
    {
      Intersection *is = &g_array_index (pd->points, Intersection, i);
      const char *kn[] = { "none", "normal", "start", "end" };

      g_print ("p1 { %lu %lu %f } p2 { %lu %lu %f } %s\n",
               is->point1.contour, is->point1.idx, is->point1.t,
               is->point2.contour, is->point2.idx, is->point2.t,
               kn[is->kind]);
    }
#endif

  for (gsize i = 0; i < pd->points->len; i++)
    {
      Intersection *is1 = &g_array_index (pd->points, Intersection, i);

      for (gsize j = i + 1; j < pd->points->len; j++)
        {
          Intersection *is2 = &g_array_index (pd->points, Intersection, j);

          if (!gsk_path_point_near (&is1->point1, &is2->point1,
                                    pd->c1_closed, pd->c1_count, pd->c1_z_is_empty,
                                    0.001))
            continue;

          if (!gsk_path_point_near (&is1->point2, &is2->point2,
                                    pd->c2_closed, pd->c2_count, pd->c2_z_is_empty,
                                    0.001))
            continue;

          if (is1->kind == GSK_PATH_INTERSECTION_NORMAL && is2->kind != GSK_PATH_INTERSECTION_NONE)
            is1->kind = GSK_PATH_INTERSECTION_NONE;
          else if (is2->kind == GSK_PATH_INTERSECTION_NORMAL && is1->kind != GSK_PATH_INTERSECTION_NONE)
            is2->kind = GSK_PATH_INTERSECTION_NONE;
        }
    }

#ifdef DEBUG
  g_print ("after collapsing\n");
  for (gsize i = 0; i < pd->points->len; i++)
    {
      Intersection *is = &g_array_index (pd->points, Intersection, i);
      const char *kn[] = { "none", "normal", "start", "end" };

      g_print ("p1 { %lu %lu %f } p2 { %lu %lu %f } %s\n",
               is->point1.contour, is->point1.idx, is->point1.t,
               is->point2.contour, is->point2.idx, is->point2.t,
               kn[is->kind]);
    }
#endif

  for (gsize i = 0; i < pd->points->len; i++)
    {
      Intersection *is1 = &g_array_index (pd->points, Intersection, i);

      for (gsize j = i + 1; j < pd->points->len; j++)
        {
          Intersection *is2 = &g_array_index (pd->points, Intersection, j);

          if (!gsk_path_point_near (&is1->point1, &is2->point1, FALSE, 0, FALSE, 0.001))
            break;

          if (!gsk_path_point_near (&is1->point2, &is2->point2,
                                    pd->c2_closed, pd->c2_count, pd->c2_z_is_empty,
                                    0.001))
            break;

          if ((is1->kind == GSK_PATH_INTERSECTION_END &&
               is2->kind == GSK_PATH_INTERSECTION_START) ||
              (is1->kind == GSK_PATH_INTERSECTION_START &&
               is2->kind == GSK_PATH_INTERSECTION_END))
            {
              is1->kind = GSK_PATH_INTERSECTION_NONE;
              is2->kind = GSK_PATH_INTERSECTION_NONE;
            }
        }
    }

#ifdef DEBUG
  g_print ("after merging segments\n");
  for (gsize i = 0; i < pd->points->len; i++)
    {
      Intersection *is = &g_array_index (pd->points, Intersection, i);
      const char *kn[] = { "none", "normal", "start", "end" };

      g_print ("p1 { %lu %lu %f } p2 { %lu %lu %f } %s\n",
               is->point1.contour, is->point1.idx, is->point1.t,
               is->point2.contour, is->point2.idx, is->point2.t,
               kn[is->kind]);
    }
#endif

  for (gsize j = 0; j < pd->points->len; j++)
    {
      Intersection *is = &g_array_index (pd->points, Intersection, j);

      if (is->kind != GSK_PATH_INTERSECTION_NONE)
        g_array_append_val (pd->all_points, *is);
    }
}

/* }}} */
/* {{{ Intersecting circle contours */

static int
circle_intersect (const graphene_point_t *center1,
                  float                   radius1,
                  const graphene_point_t *center2,
                  float                   radius2,
                  graphene_point_t        points[2])
{
  float d;
  float a, h;
  graphene_point_t m;
  graphene_vec2_t n;

  g_assert (radius1 >= 0);
  g_assert (radius2 >= 0);

  d = graphene_point_distance (center1, center2, NULL, NULL);

  if (d < fabsf (radius1 - radius2))
    return 0;

  if (d > radius1 + radius2)
    return 0;

  if (d == radius1 + radius2)
    {
      graphene_point_interpolate (center1, center2, radius1 / (radius1 + radius2), &points[0]);
      return 1;
    }

  a = (radius1*radius1 - radius2*radius2 + d*d)/(2*d);
  h = sqrtf (radius1*radius1 - a*a);

  graphene_point_interpolate (center1, center2, a/d, &m);
  graphene_vec2_init (&n, center2->y - center1->y, center1->x - center2->x);
  graphene_vec2_normalize (&n, &n);

  graphene_point_init (&points[0], m.x + graphene_vec2_get_x (&n) * h,
                                   m.y + graphene_vec2_get_y (&n) * h);
  graphene_point_init (&points[1], m.x - graphene_vec2_get_x (&n) * h,
                                   m.y - graphene_vec2_get_y (&n) * h);

  return 2;
}

static void
circle_contour_collect_intersections (const GskContour  *contour1,
                                      const GskContour  *contour2,
                                      PathIntersectData *pd)
{
  graphene_point_t center1, center2;
  float radius1, radius2;
  gboolean ccw1, ccw2;
  graphene_point_t p[2];
  int n;
  Intersection is[2];

  gsk_circle_contour_get_params (contour1, &center1, &radius1, &ccw1);
  gsk_circle_contour_get_params (contour2, &center2, &radius2, &ccw2);

  if (graphene_point_equal (&center1, &center2) && radius1 == radius2)
    {
      is[0].kind = GSK_PATH_INTERSECTION_START;
      is[0].point1.contour = pd->contour1;
      is[0].point1.idx = 1;
      is[0].point1.t = 0;
      is[0].point2.contour = pd->contour2;
      is[0].point2.idx = 1;
      is[0].point2.t = 0;

      is[1].kind = GSK_PATH_INTERSECTION_END;
      is[1].point1.contour = pd->contour1;
      is[1].point1.idx = 1;
      is[1].point1.t = 1;
      is[1].point2.contour = pd->contour2;
      is[1].point2.idx = 1;
      is[1].point2.t = 1;

      if (ccw1 != ccw2)
        {
          is[0].point2.t = 1;
          is[1].point2.t = 0;
        }

      g_array_append_val (pd->all_points, is[0]);
      g_array_append_val (pd->all_points, is[1]);

      return;
    }

  n = circle_intersect (&center1, radius1, &center2, radius2, p);

  for (int i = 0; i < n; i++)
    {
      float d;

      is[i].kind = GSK_PATH_INTERSECTION_NORMAL;
      is[i].point1.contour = pd->contour1;
      is[i].point2.contour = pd->contour2;

      gsk_contour_get_closest_point (contour1, &p[i], 1, &is[i].point1, &d);
      gsk_contour_get_closest_point (contour2, &p[i], 1, &is[i].point2, &d);
    }

  if (n == 1)
    {
      g_array_append_val (pd->all_points, is[0]);
    }
  else if (n == 2)
    {
      if (gsk_path_point_compare (&is[0].point1, &is[1].point1) < 0)
        {
          g_array_append_val (pd->all_points, is[0]);
          g_array_append_val (pd->all_points, is[1]);
        }
      else
        {
          g_array_append_val (pd->all_points, is[1]);
          g_array_append_val (pd->all_points, is[0]);
        }
    }
}

/* }}} */
/* {{{ Handling contours */

static void
contour_collect_intersections (const GskContour  *contour1,
                               const GskContour  *contour2,
                               PathIntersectData *pd)
{
  if (strcmp (gsk_contour_get_type_name (contour1), "GskCircleContour") == 0 &&
      strcmp (gsk_contour_get_type_name (contour2), "GskCircleContour") == 0)
    circle_contour_collect_intersections (contour1, contour2, pd);
  else
    default_contour_collect_intersections (contour1, contour2, pd);
}

static int
cmp_path1 (gconstpointer p1,
           gconstpointer p2)
{
  const Intersection *i1 = p1;
  const Intersection *i2 = p2;
  int i;

  i = gsk_path_point_compare (&i1->point1, &i2->point1);
  if (i != 0)
    return i;

  return gsk_path_point_compare (&i1->point2, &i2->point2);
}

static gboolean
contour_foreach_intersection (const GskContour  *contour1,
                              PathIntersectData *pd)
{
  GskBoundingBox b1, b2;

  gsk_contour_get_bounds (contour1, &b1);

  g_array_set_size (pd->all_points, 0);

  for (gsize i = 0; i < gsk_path_get_n_contours (pd->path2); i++)
    {
      const GskContour *contour2 = gsk_path_get_contour (pd->path2, i);

      gsk_contour_get_bounds (contour1, &b2);

      if (gsk_bounding_box_intersection (&b1, &b2, NULL))
        {
          pd->contour2 = i;
          pd->c2 = contour2;
          pd->c2_count = count_curves (contour2, &pd->c2_closed, &pd->c2_z_is_empty);

          contour_collect_intersections (contour1, contour2, pd);
        }
    }

  g_array_sort (pd->all_points, cmp_path1);

#ifdef DEBUG
  g_print ("after sorting\n");
  for (gsize i = 0; i < pd->all_points->len; i++)
    {
      Intersection *is = &g_array_index (pd->all_points, Intersection, i);
      const char *kn[] = { "none", "normal", "start", "end" };

      g_print ("p1 { %lu %lu %f } p2 { %lu %lu %f } %s\n",
               is->point1.contour, is->point1.idx, is->point1.t,
               is->point2.contour, is->point2.idx, is->point2.t,
               kn[is->kind]);
    }
#endif

  for (gsize i = 0; i + 1 < pd->all_points->len; i++)
    {
      Intersection *is1 = &g_array_index (pd->all_points, Intersection, i);
      Intersection *is2 = &g_array_index (pd->all_points, Intersection, i + 1);

      if (gsk_path_point_equal (&is1->point1, &is2->point1) &&
          gsk_path_point_equal (&is1->point2, &is2->point2))
        {
          if (is1->kind == GSK_PATH_INTERSECTION_END &&
              is2->kind == GSK_PATH_INTERSECTION_START)
            {
              is1->kind = GSK_PATH_INTERSECTION_NONE;
              is2->kind = GSK_PATH_INTERSECTION_NONE;
            }
          else
            {
              is2->kind = MAX (is1->kind, is2->kind);
              is1->kind = GSK_PATH_INTERSECTION_NONE;
            }
        }
    }

#ifdef DEBUG
  g_print ("emitting\n");
  for (gsize i = 0; i < pd->all_points->len; i++)
    {
      Intersection *is = &g_array_index (pd->all_points, Intersection, i);
      const char *kn[] = { "none", "normal", "start", "end" };

      g_print ("p1 { %lu %lu %f } p2 { %lu %lu %f } %s\n",
               is->point1.contour, is->point1.idx, is->point1.t,
               is->point2.contour, is->point2.idx, is->point2.t,
               kn[is->kind]);
    }
#endif

  for (gsize i = 0; i < pd->all_points->len; i++)
    {
      Intersection *is = &g_array_index (pd->all_points, Intersection, i);

      if (is->kind != GSK_PATH_INTERSECTION_NONE)
        {

          if (!pd->func (pd->path1, &is->point1, pd->path2, &is->point2, is->kind, pd->data))
            return FALSE;
        }
    }

  return TRUE;
}

/* }}} */
/* {{{ Public API */

/**
 * gsk_path_foreach_intersection:
 * @path1: the first path
 * @path2: (nullable): the second path
 * @func: (scope call) (closure user_data): the function to call for intersections
 * @user_data: (nullable): user data passed to @func
 *
 * Finds intersections between two paths.
 *
 * This function finds intersections between @path1 and @path2,
 * and calls @func for each of them, in increasing order for @path1.
 *
 * If @path2 is not provided or equal to @path1, the function finds
 * non-trivial self-intersections of @path1.
 *
 * When segments of the paths coincide, the callback is called once
 * for the start of the segment, with @GSK_PATH_INTERSECTION_START, and
 * once for the end of the segment, with @GSK_PATH_INTERSECTION_END.
 * Note that other intersections may occur between the start and end
 * of such a segment.
 *
 * If @func returns `FALSE`, the iteration is stopped.
 *
 * Returns: `FALSE` if @func returned FALSE`, `TRUE` otherwise.
 *
 * Since: 4.20
 */
gboolean
gsk_path_foreach_intersection (GskPath                 *path1,
                               GskPath                 *path2,
                               GskPathIntersectionFunc  func,
                               gpointer                 data)
{
  PathIntersectData pd = {
    .path1 = path1,
    .path2 = path2 ? path2 : path1,
    .func = func,
    .data = data,
  };
  gboolean ret;

  pd.points = g_array_new (FALSE, FALSE, sizeof (Intersection));
  pd.all_points = g_array_new (FALSE, FALSE, sizeof (Intersection));

  ret = TRUE;
  for (gsize i = 0; i < gsk_path_get_n_contours (path1); i++)
    {
      const GskContour *contour1 = gsk_path_get_contour (path1, i);

      pd.contour1 = i;
      pd.c1 = contour1;
      pd.c1_count = count_curves (contour1, &pd.c1_closed, &pd.c1_z_is_empty);
      pd.idx1 = 0;

      if (!contour_foreach_intersection (contour1, &pd))
        {
          ret = FALSE;
          break;
        }
    }

  g_array_unref (pd.points);
  g_array_unref (pd.all_points);

  return ret;
}

/* }}} */
/* vim:set foldmethod=marker expandtab: */

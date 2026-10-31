/*
 * Copyright © 2026 Red Hat, Inc.
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

#include "gskcontourprivate.h"
#include "gskroundedrectprivate.h"

/* Some svg simplifiers (tinysvg, looking at you) replace
 * perfectly fine basic shapes like rects and circles with
 * paths. Since we can deal much more efficiently with those
 * basic shapes, we make some effort to recover them.
 */

static gboolean
path_is_rect (GskPathOperation *ops,
              size_t            n_ops,
              graphene_point_t *points,
              size_t            n_points,
              graphene_rect_t  *rect)
{
  /* Look for the path produced by an axis-aligned rectangle: mlllz */

  if (n_ops != 5)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_LINE ||
      ops[2] != GSK_PATH_LINE ||
      ops[3] != GSK_PATH_LINE ||
      ops[4] != GSK_PATH_CLOSE)
    return FALSE;

  if (!((points[0].y == points[1].y &&
         points[1].x == points[2].x &&
         points[2].y == points[3].y &&
         points[3].x == points[0].x) ||
        (points[0].x == points[1].x &&
         points[1].y == points[2].y &&
         points[2].x == points[3].x &&
         points[3].y == points[0].y)))
    return FALSE;

  if (points[0].x == points[1].x)
    {
      rect->origin.x = MIN (points[0].x, points[2].x);
      rect->size.width = MAX (points[0].x, points[2].x) - rect->origin.x;
    }
  else
    {
      rect->origin.x = MIN (points[0].x, points[1].x);
      rect->size.width = MAX (points[0].x, points[1].x) - rect->origin.x;
    }
  if (points[0].y == points[1].y)
    {
      rect->origin.y = MIN (points[0].y, points[2].y);
      rect->size.height = MAX (points[0].y, points[2].y) - rect->origin.y;
    }
  else
    {
      rect->origin.y = MIN (points[0].y, points[1].y);
      rect->size.height = MAX (points[0].y, points[1].y) - rect->origin.y;
    }

  return TRUE;
}

#define in_order3(a, b, c) ((a <= b && b <= c) || (b <= a && c <= b))
#define in_order4(a, b, c, d) \
  ((a <= b && b <= c && b <= c) || \
   (b <= a && c <= b && c <= b))
#define in_order5(a, b, c, d, e) \
  ((a <= b && b <= c && c <= d && d <= e) || \
   (a >= b && b >= c && c >= d && d >= e))
#define in_order6(a, b, c, d, e, f) \
  ((a <= b && b <= c && c <= d && d <= e && e <= f) || \
   (a >= b && b >= c && c >= d && d >= e && e >= f))

#define equal3(a, b, c) (a == b && b == c)
#define equal4(a, b, c, d) (a == b && b == c && c == d)

static const double quarter_circle_d = (M_SQRT2 - 1) * 4 / 3;
static const double quarter_circle_eps = 0.05;

static inline gboolean
is_quarter_ellipse (const graphene_point_t *p1,
                    const graphene_point_t *p2,
                    const graphene_point_t *p3,
                    const graphene_point_t *p4)
{
  return G_APPROX_VALUE (p2->x - p1->x, quarter_circle_d * (p4->x - p1->x), quarter_circle_eps) &&
         G_APPROX_VALUE (p4->y - p3->y, quarter_circle_d * (p4->y - p1->y), quarter_circle_eps);
}

static inline GskCorner
quarter_ellipse_corner (const graphene_point_t *p1,
                        const graphene_point_t *p4)
{
  if (p1->x < p4->x)
    {
      if (p1->y < p4->y)
        return GSK_CORNER_TOP_RIGHT;
      else
        return GSK_CORNER_BOTTOM_RIGHT;
    }
  else
    {
      if (p1->y < p4->y)
        return GSK_CORNER_TOP_LEFT;
      else
        return GSK_CORNER_BOTTOM_LEFT;
    }
}

static gboolean
path_is_circle (GskPathOperation *ops,
                size_t            n_ops,
                graphene_point_t *points,
                size_t            n_points,
                GskRoundedRect   *rect)
{
  /* Look for the path produced by the common way
   * to encode a circle: mccccz.
   *
   * See https://spencermortensen.com/articles/bezier-circle/
   *
   * There are of course many other ways to encode circles
   * that we don't find. Such is life.
   *
   * Harmlessly, we also accept a trailing m
   */
  double r;

  if (n_ops != 6)
    return FALSE;

  if (n_points != 14)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_CUBIC ||
      ops[2] != GSK_PATH_CUBIC ||
      ops[3] != GSK_PATH_CUBIC ||
      ops[4] != GSK_PATH_CUBIC ||
      ops[5] != GSK_PATH_CLOSE)
    return FALSE;

  if (!(points[0].x == points[12].x &&
        points[0].y == points[12].y))
    return FALSE;

  if (!(equal3 (points[11].x, points[0].x, points[1].x) &&
        equal3 (points[2].y, points[3].y, points[4].y) &&
        equal3 (points[5].x, points[6].x, points[7].x) &&
        equal3 (points[8].y, points[9].y, points[10].y)))
    return FALSE;
  if (!(points[11].y == points[7].y &&
        points[0].y == points[6].y &&
        points[1].y == points[5].y &&
        points[2].x == points[10].x &&
        points[3].x == points[9].x &&
        points[4].x == points[8].x))
    return FALSE;

  if (!in_order5 (points[10].y, points[11].y, points[0].y, points[1].y, points[2].y))
    return FALSE;

  if (!in_order5 (points[1].x, points[2].x, points[3].x, points[4].x, points[5].x))
    return FALSE;

  if (points[0].y - points[3].y != points[9].y - points[0].y)
    return FALSE;

  if (points[3].x - points[6].x != points[0].x - points[3].x)
    return FALSE;

  if (fabs (points[0].y - points[3].y) != fabs (points[0].x - points[3].x))
    return FALSE;

  r = fabs (points[0].y - points[3].y);

  if (points[0].y - points[1].y != points[11].y - points[12].y)
    return FALSE;

  if (points[2].x - points[3].x != points[3].x - points[4].x)
    return FALSE;

  if (!G_APPROX_VALUE (fabs (points[0].y - points[1].y), fabs (points[2].x - points[3].x), 0.01))
    return FALSE;

  if (!G_APPROX_VALUE (fabs (points[0].y - points[1].y), quarter_circle_d * r, quarter_circle_eps))
    return FALSE;

  gsk_rounded_rect_init_from_ellipse (rect,
                                      MIN (points[6].x, points[0].x) + r,
                                      MIN (points[9].y, points[3].y) + r,
                                      r, r);

  return TRUE;
}

#define swap(a, b) { tmp = a; a = b; b = tmp; }

static gboolean
path_is_circle2 (GskPathOperation *ops,
                 size_t            n_ops,
                 graphene_point_t *points,
                 size_t            n_points,
                 GskRoundedRect   *rect)
{
  graphene_point_t pts[14];
  float tmp;

  if (n_ops != 6)
    return FALSE;

  if (n_points != 14)
    return FALSE;

  for (unsigned int i = 0; i < 14; i++)
    {
      pts[i].x = points[i].y;
      pts[i].y = points[i].x;
    }

  if (!path_is_circle (ops, n_ops, pts, n_points, rect))
    return FALSE;

  swap (rect->bounds.origin.x, rect->bounds.origin.y);
  swap (rect->bounds.size.width, rect->bounds.size.height);

  return TRUE;
}

static gboolean
path_is_precise_circle (GskPathOperation *ops,
                        size_t            n_ops,
                        graphene_point_t *points,
                        size_t            n_points,
                        GskRoundedRect   *rect)
{
  if (n_ops != 6)
    return FALSE;

  if (n_points != 14)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_CONIC ||
      ops[2] != GSK_PATH_CONIC ||
      ops[3] != GSK_PATH_CONIC ||
      ops[4] != GSK_PATH_CONIC ||
      ops[5] != GSK_PATH_CLOSE)
    return FALSE;

  if (!(points[0].x == points[12].x && points[0].y == points[12].y))
    return FALSE;

  if (!(equal3 (points[10].x, points[12].x, points[1].x) &&
        equal3 (points[1].y, points[3].y, points[4].y) &&
        equal3 (points[4].x, points[6].x, points[7].x) &&
        equal3 (points[7].y, points[9].y, points[10].y)))
    return FALSE;

  if (points[3].x != points[9].x || points[0].y != points[6].y)
    return FALSE;

  if (!(equal4 (points[2].x, points[5].x, points[8].x, points[11].x)))
    return FALSE;

  if (points[2].x != (float) M_SQRT1_2)
    return FALSE;

  gsk_rounded_rect_init_from_ellipse (rect,
                                      points[3].x, points[3].y,
                                      points[0].x - points[3].x,
                                      points[0].x - points[3].x);

  return TRUE;
}

static gboolean
rounded_rect_from_points2 (graphene_point_t *points,
                           GskRoundedRect   *rect)
{
  GskCorner c;

  /* points are assumed to be for an mlclclclcz contour */

  if (points[0].x != points[16].x ||
      points[0].y != points[16].y)
    return FALSE;

  if (!(equal4 (points[15].y, points[0].y, points[1].y, points[2].y) &&
        equal4 (points[3].x, points[4].x, points[5].x, points[6].x) &&
        equal4 (points[7].y, points[8].y, points[9].y, points[10].y) &&
        equal4 (points[11].x, points[12].x, points[13].x, points[14].x)))
    return FALSE;

  /* We match both cw and ccw */
  if (!in_order6 (points[14].x, points[15].x, points[0].x, points[1].x, points[2].x, points[3].x))
    return FALSE;

  if (!in_order6 (points[2].y, points[3].y, points[4].y, points[5].y, points[6].y, points[7].y))
    return FALSE;

  graphene_rect_init (&rect->bounds,
                      MIN (points[4].x, points[13].x),
                      MIN (points[8].y, points[1].y),
                      fabs (points[13].x - points[4].x),
                      fabs (points[8].y - points[1].y));

  if (!is_quarter_ellipse (&points[1], &points[2], &points[3], &points[4]))
    return FALSE;

  c = quarter_ellipse_corner (&points[1], &points[4]);
  rect->corner[c].width = fabs (points[4].x - points[1].x);
  rect->corner[c].height = fabs (points[4].y - points[1].y);

  if (!is_quarter_ellipse (&points[8], &points[7], &points[6], &points[5]))
    return FALSE;

  c = quarter_ellipse_corner (&points[8], &points[5]);
  rect->corner[c].width = fabs (points[8].x - points[5].x);
  rect->corner[c].height = fabs (points[8].y - points[5].y);

  if (!is_quarter_ellipse (&points[9], &points[10], &points[11], &points[12]))
    return FALSE;

  c = quarter_ellipse_corner (&points[9], &points[12]);
  rect->corner[c].width = fabs (points[9].x - points[12].x);
  rect->corner[c].height = fabs (points[9].y - points[12].y);

  if (!is_quarter_ellipse (&points[16], &points[15], &points[14], &points[13]))
    return FALSE;

  c = quarter_ellipse_corner (&points[16], &points[13]);
  rect->corner[c].width = fabs (points[16].x - points[13].x);
  rect->corner[c].height = fabs (points[16].y - points[13].y);

  return TRUE;
}

static gboolean
rounded_rect_from_points (graphene_point_t *points,
                          GskRoundedRect   *rect)
{
  if (rounded_rect_from_points2 (points, rect))
    {
      return TRUE;
    }
  else
    {
      graphene_point_t pts[18];

      for (unsigned int i = 0; i < 18; i++)
        {
          pts[i].x = points[i].y;
          pts[i].y = points[i].x;
        }

      if (rounded_rect_from_points2 (pts, rect))
        {
          float tmp;

          swap (rect->bounds.origin.x, rect->bounds.origin.y);
          swap (rect->bounds.size.width, rect->bounds.size.height);
          swap (rect->corner[0].width, rect->corner[0].height);
          swap (rect->corner[1].width, rect->corner[1].height);
          swap (rect->corner[2].width, rect->corner[2].height);
          swap (rect->corner[3].width, rect->corner[3].height);

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
rounded_rect_from_points3 (graphene_point_t *points,
                           GskRoundedRect   *rect)
{
  GskCorner c;

  /* points are assumed to be for an mlolololoz contour */

  if (points[0].x != points[16].x ||
      points[0].y != points[16].y)
    return FALSE;

  if (!(equal4 (points[14].y, points[0].y, points[1].y, points[2].y) &&
        equal4 (points[2].x, points[4].x, points[5].x, points[6].x) &&
        equal4 (points[6].y, points[8].y, points[9].y, points[10].y) &&
        equal4 (points[10].x, points[12].x, points[13].x, points[14].x)))
    return FALSE;

  if (!in_order4 (points[14].x, points[0].x, points[1].x, points[2].x))
    return FALSE;

  if (!in_order4 (points[2].y, points[4].y, points[5].y, points[6].y))
    return FALSE;

  graphene_rect_init (&rect->bounds,
                      MIN (points[14].x, points[2].x),
                      points[14].y,
                      fabs (points[2].x - points[14].x),
                      fabs (points[10].y - points[14].y));

  if (points[1].x < points[4].x)
    {
      if (points[1].y < points[4].y)
        c = GSK_CORNER_TOP_RIGHT;
      else
        c = GSK_CORNER_BOTTOM_RIGHT;
    }
  else
    {
      if (points[1].y < points[4].y)
        c = GSK_CORNER_TOP_LEFT;
      else
        c = GSK_CORNER_BOTTOM_LEFT;
    }

  rect->corner[c].width = fabs (points[4].x - points[1].x);
  rect->corner[c].height = fabs (points[4].y - points[1].y);

  if (points[8].x < points[5].x)
    {
      if (points[5].y < points[8].y)
        c = GSK_CORNER_BOTTOM_RIGHT;
      else
        c = GSK_CORNER_TOP_RIGHT;
    }
  else
    {
      if (points[5].y < points[8].y)
        c = GSK_CORNER_BOTTOM_LEFT;
      else
        c = GSK_CORNER_TOP_LEFT;
    }

  rect->corner[c].width = fabs (points[5].x - points[8].x);
  rect->corner[c].height = fabs (points[8].y - points[5].y);

  if (points[12].x < points[9].x)
    {
      if (points[12].y < points[9].y)
        c = GSK_CORNER_BOTTOM_LEFT;
      else
        c = GSK_CORNER_TOP_LEFT;
    }
  else
    {
      if (points[12].y < points[9].y)
        c = GSK_CORNER_BOTTOM_RIGHT;
      else
        c = GSK_CORNER_TOP_RIGHT;
    }

  rect->corner[c].width = fabs (points[9].x - points[12].x);
  rect->corner[c].height = fabs (points[9].y - points[12].y);

  if (points[13].x < points[16].x)
    {
      if (points[16].y < points[13].y)
        c = GSK_CORNER_TOP_LEFT;
      else
        c = GSK_CORNER_BOTTOM_LEFT;
    }
  else
    {
      if (points[16].y < points[13].y)
        c = GSK_CORNER_TOP_RIGHT;
      else
        c = GSK_CORNER_BOTTOM_RIGHT;
    }

  rect->corner[c].width = fabs (points[16].x - points[13].x);
  rect->corner[c].height = fabs (points[13].y - points[16].y);

  return TRUE;
}

static gboolean
path_is_precise_rounded_rect (GskPathOperation *ops,
                              size_t            n_ops,
                              graphene_point_t *points,
                              size_t            n_points,
                              GskRoundedRect   *rect)
{
  if (n_ops != 10)
    return FALSE;

  if (n_points != 18)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_LINE ||
      ops[2] != GSK_PATH_CONIC ||
      ops[3] != GSK_PATH_LINE ||
      ops[4] != GSK_PATH_CONIC ||
      ops[5] != GSK_PATH_LINE ||
      ops[6] != GSK_PATH_CONIC ||
      ops[7] != GSK_PATH_LINE ||
      ops[8] != GSK_PATH_CONIC ||
      ops[9] != GSK_PATH_CLOSE)
    return FALSE;

  if (!(equal4 (points[3].x, points[7].x, points[11].x, points[15].x)))
    return FALSE;

  if (points[3].x != (float) M_SQRT1_2)
    return FALSE;

  return rounded_rect_from_points3 (points, rect);
}

static gboolean
path_is_rounded_rect (GskPathOperation *ops,
                      size_t            n_ops,
                      graphene_point_t *points,
                      size_t            n_points,
                      GskRoundedRect   *rect)
{
  if (n_ops != 10)
    return FALSE;

  if (n_points != 18)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_LINE ||
      ops[2] != GSK_PATH_CUBIC ||
      ops[3] != GSK_PATH_LINE ||
      ops[4] != GSK_PATH_CUBIC ||
      ops[5] != GSK_PATH_LINE ||
      ops[6] != GSK_PATH_CUBIC ||
      ops[7] != GSK_PATH_LINE ||
      ops[8] != GSK_PATH_CUBIC ||
      ops[9] != GSK_PATH_CLOSE)
    return FALSE;

  return rounded_rect_from_points (points, rect);
}

static gboolean
path_is_rounded_rect2 (GskPathOperation *ops,
                       size_t            n_ops,
                       graphene_point_t *points,
                       size_t            n_points,
                       GskRoundedRect   *rect)
{
  graphene_point_t pts[18];

  if (n_ops != 10)
    return FALSE;

  if (n_points != 18)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_CUBIC ||
      ops[2] != GSK_PATH_LINE ||
      ops[3] != GSK_PATH_CUBIC ||
      ops[4] != GSK_PATH_LINE ||
      ops[5] != GSK_PATH_CUBIC ||
      ops[6] != GSK_PATH_LINE ||
      ops[7] != GSK_PATH_CUBIC ||
      ops[8] != GSK_PATH_LINE ||
      ops[9] != GSK_PATH_CLOSE)
    return FALSE;

  /* rotate the points to go from mclclclclz to mlclclclcz */
  pts[0] = points[15];
  memcpy (pts + 1, points, sizeof (graphene_point_t) * 16);
  pts[17] = pts[0];

  if (rounded_rect_from_points (pts, rect))
    return TRUE;

  /* flip point order of points to go from mclclclclz to mlclclclcz */
  for (unsigned int i = 0; i <= 16; i++)
    pts[i] = points[16 - i];

  pts[17] = points[0];

  return rounded_rect_from_points (pts, rect);
}

static gboolean
path_is_pill (GskPathOperation *ops,
              size_t            n_ops,
              graphene_point_t *points,
              size_t            n_points,
              GskRoundedRect   *rect)
{
  graphene_point_t pts[18];

  /* Check for the 'horizontal pill' shape that results from
   * omitting the vertical lines in a rounded rect
   */

  if (n_ops != 8)
    return FALSE;

  if (n_points != 16)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_LINE ||
      ops[2] != GSK_PATH_CUBIC ||
      ops[3] != GSK_PATH_CUBIC ||
      ops[4] != GSK_PATH_LINE ||
      ops[5] != GSK_PATH_CUBIC ||
      ops[6] != GSK_PATH_CUBIC ||
      ops[7] != GSK_PATH_CLOSE)
    return FALSE;

  /* duplicate points 4 and 11 to from mlcclccz to mlclclclcz */
  memcpy (pts, points, sizeof (graphene_point_t) * 5);
  memcpy (pts + 5, points + 4, sizeof (graphene_point_t) * 8);
  memcpy (pts + 13, points + 11, sizeof (graphene_point_t) * 5);

  return rounded_rect_from_points (pts, rect);
}

static gboolean
path_is_pill2 (GskPathOperation *ops,
               size_t            n_ops,
               graphene_point_t *points,
               size_t            n_points,
               GskRoundedRect   *rect)
{
  graphene_point_t pts[18];

  if (n_ops != 8)
    return FALSE;

  if (n_points != 16)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_CUBIC ||
      ops[2] != GSK_PATH_CUBIC ||
      ops[3] != GSK_PATH_LINE ||
      ops[4] != GSK_PATH_CUBIC ||
      ops[5] != GSK_PATH_CUBIC ||
      ops[6] != GSK_PATH_LINE ||
      ops[7] != GSK_PATH_CLOSE)
    return FALSE;

  pts[0] = points[13];
  memcpy (pts + 1, points, sizeof (graphene_point_t) * 4);
  memcpy (pts + 5, points + 4, sizeof (graphene_point_t) * 8);
  memcpy (pts + 13, points + 11, sizeof (graphene_point_t) * 4);
  pts[17] = points[13];

  return rounded_rect_from_points (pts, rect);
}

static gboolean
path_is_pill3 (GskPathOperation *ops,
               size_t            n_ops,
               graphene_point_t *points,
               size_t            n_points,
               GskRoundedRect   *rect)
{
  graphene_point_t pts[18];

  if (n_ops != 8)
    return FALSE;

  if (n_points != 16)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_CUBIC ||
      ops[2] != GSK_PATH_LINE ||
      ops[3] != GSK_PATH_CUBIC ||
      ops[4] != GSK_PATH_CUBIC ||
      ops[5] != GSK_PATH_LINE ||
      ops[6] != GSK_PATH_CUBIC ||
      ops[7] != GSK_PATH_CLOSE)
    return FALSE;

  memcpy (pts, points + 3, sizeof (graphene_point_t) * 5);
  memcpy (pts + 5, points + 7, sizeof (graphene_point_t) * 8);
  memcpy (pts + 13, points, sizeof (graphene_point_t) * 4);
  pts[17] = points[3];

  return rounded_rect_from_points (pts, rect);
}

/* Look for a simple shape that is disguised
 * as a path. The resulting shape is returned
 * in @rect.
 */
GskPathClassification
gsk_path_classify (GskPath        *path,
                   GskRoundedRect *rect)
{
  const GskContour *contour;
  graphene_point_t center;
  float radius;
  gboolean ccw;
  GskPathOperation ops[10];
  graphene_point_t points[18];
  size_t n_ops;
  size_t n_points;

  if (gsk_path_is_empty (path))
    return GSK_PATH_EMPTY;

  if (gsk_path_get_n_contours (path) > 2 ||
      (gsk_path_get_n_contours (path) == 2 &&
       gsk_contour_get_standard_ops (gsk_path_get_contour (path, 1), 0, NULL) > 1))
    return GSK_PATH_GENERAL;

  contour = gsk_path_get_contour (path, 0);

  if (gsk_contour_get_rect (contour, &rect->bounds))
    {
      memset (rect->corner, 0, sizeof (graphene_size_t) * 4);
      return GSK_PATH_RECT;
    }
  else if (gsk_contour_get_rounded_rect (contour, rect))
    {
      return GSK_PATH_ROUNDED_RECT;
    }
  else if (gsk_contour_get_circle (contour, &center, &radius, &ccw))
    {
      gsk_rounded_rect_init_from_ellipse (rect,
                                          center.x, center.y,
                                          radius, radius);
      return GSK_PATH_CIRCLE;
    }

  n_ops = gsk_contour_get_standard_ops (contour, G_N_ELEMENTS (ops), ops);
  n_points = gsk_contour_get_standard_points (contour, G_N_ELEMENTS (points), points);

  if (path_is_rect (ops, n_ops, points, n_points, &rect->bounds))
    return GSK_PATH_RECT;
  else if (path_is_precise_circle (ops, n_ops, points, n_points, rect))
    return GSK_PATH_CIRCLE;
  else if (path_is_precise_rounded_rect (ops, n_ops, points, n_points, rect))
    return GSK_PATH_ROUNDED_RECT;
  else if (path_is_circle (ops, n_ops, points, n_points, rect) ||
           path_is_circle2 (ops, n_ops, points, n_points, rect))
    return GSK_PATH_APPROXIMATE_CIRCLE;
  else if (path_is_rounded_rect (ops, n_ops, points, n_points, rect) ||
           path_is_rounded_rect2 (ops, n_ops, points, n_points, rect) ||
           path_is_pill (ops, n_ops, points, n_points, rect) ||
           path_is_pill2 (ops, n_ops, points, n_points, rect) ||
           path_is_pill3 (ops, n_ops, points, n_points, rect))
    return GSK_PATH_APPROXIMATE_ROUNDED_RECT;

  return GSK_PATH_GENERAL;
}

/* vim:set foldmethod=marker: */

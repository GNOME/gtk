#include "gtksvgpathutilsprivate.h"

#include "gsk/gskroundedrectprivate.h"
#include "gsk/gskpathprivate.h"
#include "gsk/gskcontourprivate.h"

/* {{{ Path transformation */

typedef struct
{
  GskPathBuilder *builder;
  GskTransform *transform;
} PathTransformData;

static gboolean
add_op_transformed (GskPathOperation        op,
                    const graphene_point_t *_pts,
                    size_t                   n_pts,
                    float                   weight,
                    gpointer                user_data)
{
  PathTransformData *t = user_data;
  graphene_point_t pts[4];

  for (unsigned int i = 0; i < n_pts; i++)
    gsk_transform_transform_point (t->transform, &_pts[i], &pts[i]);

  gsk_path_builder_add_op (t->builder, op, pts, n_pts, weight);

  return TRUE;
}

/* Transform a path by applying the transform
 * to every point.
 */
GskPath *
svg_transform_path (GskTransform *transform,
                    GskPath      *path)
{
  PathTransformData data;

  data.builder = gsk_path_builder_new ();
  data.transform = transform;

  gsk_path_foreach (path, (GskPathForeachFlags) -1, add_op_transformed, &data);

  return gsk_path_builder_free_to_path (data.builder);
}

/* }}} */
/* {{{ Path decomposition */

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

#define in_order5(a, b, c, d, e) \
  ((a <= b && b <= c && c <= d && d <= e) || \
   (a >= b && b >= c && c >= d && d >= e))
#define in_order6(a, b, c, d, e, f) \
  ((a <= b && b <= c && c <= d && d <= e && e <= f) || \
   (a >= b && b >= c && c >= d && d >= e && e >= f))

#define equal3(a, b, c) (a == b && b == c)
#define equal4(a, b, c, d) (a == b && b == c && c == d)

static const double quarter_circle_d = (M_SQRT2 - 1) * 4 / 3;

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

  if (!G_APPROX_VALUE (fabs (points[0].y - points[1].y), quarter_circle_d * r, 0.01))
    return FALSE;

  gsk_rounded_rect_init_uniform (rect,
                                 MIN (points[6].x, points[0].x),
                                 MIN (points[9].y, points[3].y),
                                 2 * r, 2 * r,
                                 r);

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

  if (!(G_APPROX_VALUE (points[2].x - points[1].x, quarter_circle_d * (points[4].x - points[1].x), 0.01) &&
        G_APPROX_VALUE (points[4].y - points[3].y, quarter_circle_d * (points[4].y - points[1].y), 0.01)))
    return FALSE;

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

  if (!(G_APPROX_VALUE (points[7].x - points[8].x, quarter_circle_d * (points[5].x - points[8].x), 0.01) &&
        G_APPROX_VALUE (points[6].y - points[5].y, quarter_circle_d * (points[8].y - points[5].y), 0.01)))
    return FALSE;

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

  if (!(G_APPROX_VALUE (points[9].x - points[10].x, quarter_circle_d * (points[9].x - points[12].x), 0.01) &&
        G_APPROX_VALUE (points[11].y - points[12].y, quarter_circle_d * (points[9].y - points[12].y), 0.01)))
    return FALSE;

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

  if (!(G_APPROX_VALUE (points[16].x - points[15].x, quarter_circle_d * (points[16].x - points[13].x), 0.01) &&
        G_APPROX_VALUE (points[13].y - points[14].y, quarter_circle_d * (points[13].y - points[16].y), 0.01)))
    return FALSE;

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

  /* rotate the points to go from mclclclclz to mlclclclclz */
  pts[0] = points[15];
  memcpy (pts + 1, points, sizeof (graphene_point_t) * 16);
  pts[17] = pts[0];

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
SvgPathClassification
svg_path_classify (GskPath        *path,
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
    return PATH_EMPTY;

  if (gsk_path_get_n_contours (path) > 2 ||
      (gsk_path_get_n_contours (path) == 2 &&
       gsk_contour_get_standard_ops (gsk_path_get_contour (path, 1), 0, NULL) > 1))
    return PATH_GENERAL;

  contour = gsk_path_get_contour (path, 0);

  if (gsk_contour_get_rect (contour, &rect->bounds))
    return PATH_RECT;
  else if (gsk_contour_get_rounded_rect (contour, rect))
    return PATH_ROUNDED_RECT;
  else if (gsk_contour_get_circle (contour, &center, &radius, &ccw))
    {
      graphene_rect_init (&rect->bounds,
                          center.x - radius,
                          center.y - radius,
                          2 * radius,
                          2 * radius);
      rect->corner[0].width = rect->corner[0].height = radius;
      rect->corner[1].width = rect->corner[1].height = radius;
      rect->corner[2].width = rect->corner[2].height = radius;
      rect->corner[3].width = rect->corner[3].height = radius;
      return PATH_CIRCLE;
    }

  n_ops = gsk_contour_get_standard_ops (contour, G_N_ELEMENTS (ops), ops);
  n_points = gsk_contour_get_standard_points (contour, G_N_ELEMENTS (points), points);

  if (path_is_rect (ops, n_ops, points, n_points, &rect->bounds))
    return PATH_RECT;
  else if (path_is_circle (ops, n_ops, points, n_points, rect) ||
           path_is_circle2 (ops, n_ops, points, n_points, rect))
    return PATH_CIRCLE;
  else if (path_is_rounded_rect (ops, n_ops, points, n_points, rect) ||
           path_is_rounded_rect2 (ops, n_ops, points, n_points, rect) ||
           path_is_pill (ops, n_ops, points, n_points, rect) ||
           path_is_pill2 (ops, n_ops, points, n_points, rect) ||
           path_is_pill3 (ops, n_ops, points, n_points, rect))
    return PATH_ROUNDED_RECT;

  return PATH_GENERAL;
}

/* }}} */
/* {{{ Snapshotting */

void
svg_snapshot_push_fill (GtkSnapshot *snapshot,
                        GskPath     *path,
                        GskFillRule  rule)
{
  GskRoundedRect rect = { 0, };

  switch (svg_path_classify (path, &rect))
    {
    case PATH_RECT:
      gtk_snapshot_push_clip (snapshot, &rect.bounds);
      break;
    case PATH_ROUNDED_RECT:
    case PATH_CIRCLE:
      gtk_snapshot_push_rounded_clip (snapshot, &rect);
      break;
    case PATH_GENERAL:
      gtk_snapshot_push_fill (snapshot, path, rule);
      break;
    case PATH_EMPTY:
      gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, 0, 0));
      break;
    default:
      g_assert_not_reached ();
    }
}

/* }}} */

/* vim:set foldmethod=marker: */


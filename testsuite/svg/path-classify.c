#include <gtk/gtk.h>
#include "gsk/gskroundedrectprivate.h"
#include "gsk/gskpathprivate.h"
#include "gsk/gskcontourprivate.h"

/* Keep in sync with gtksvg.c */
/* {{{ Path decomposition */

typedef enum
{
  PATH_EMPTY,
  PATH_RECT,
  PATH_ROUNDED_RECT,
  PATH_CIRCLE,
  PATH_GENERAL,
} PathClassification;

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

static PathClassification
classify_path (GskPath        *path,
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

typedef struct
{
  const char *path_str;
  PathClassification expected;
} TestData;

static void
test_data_free (gpointer data)
{
  TestData *d = data;
  g_free ((char *) d->path_str);
  g_free (d);
}

static void
test_path (gconstpointer data)
{
  const TestData *d = data;
  GskPath *path;
  PathClassification result;
  GskRoundedRect rect;
  const char *kind[] = {
    "empty", "rect", "rounded", "circle", "general"
  };

  if (g_test_verbose ())
    g_test_message ("Classify %s", d->path_str);

  path = gsk_path_parse (d->path_str);

  result = classify_path (path, &rect);
  if (result != d->expected)
    g_test_fail_printf ("Expected '%s', got '%s'", kind[d->expected], kind[result]);

  gsk_path_unref (path);
}

static void
add_test (int                 pos,
          const char         *path_str,
          PathClassification  c)
{
  TestData *data;
  char *test_name;

  data = g_new (TestData, 1);
  data->path_str = g_strdup (path_str);
  data->expected = c;

  test_name = g_strdup_printf ("/path/classify/%d", pos);
  g_test_add_data_func_full (test_name, data, test_path, test_data_free);
  g_free (test_name);
}

static void
add_path_test (int                 pos,
               GskPath            *path,
               PathClassification  c)
{
  char *path_str;

  path_str = gsk_path_to_string (path);
  add_test (pos, path_str, c);
  g_free (path_str);
}

static void
add_rounded_rect_test (int                 pos,
                       GskRoundedRect     *rr,
                       PathClassification  c)
{
  GskPathBuilder *builder;
  GskPath *path;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rounded_rect (builder, rr);
  path = gsk_path_builder_free_to_path (builder);
  add_path_test (pos, path, c);
  gsk_path_unref (path);
}

static void
add_rect_test (int                 pos,
               graphene_rect_t    *rect,
               PathClassification  c)
{
  GskPathBuilder *builder;
  GskPath *path;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, rect);
  path = gsk_path_builder_free_to_path (builder);
  add_path_test (pos, path, c);
  gsk_path_unref (path);
}

#define RR(x,y,w,h,r) \
  (GskRoundedRect) { \
  .bounds = { \
    .origin = { x, y, }, \
    .size = { w, h, }, \
  }, \
  .corner[0] = { r, r, }, \
  .corner[1] = { r, r, }, \
  .corner[2] = { r, r, }, \
  .corner[3] = { r, r, }, \
}

static TestData tests[] = {
  { "", PATH_EMPTY },
  { "M10,10h20M20,20", PATH_GENERAL },
  /* the following are from org.gnome.Loupe.svg */
  { "m 2 2 h 124 v 124 h -124 z m 0 0", PATH_RECT },
  { "M 106 20 L 22 20 C 20.8945312 20, 20 20.8945312, 20 22 L 20 86 C 20 87.1054688, 20.8945312 88, 22 88 L 106 88 C 107.105469 88, 108 87.1054688, 108 86 L 108 22 C 108 20.8945312, 107.105469 20, 106 20 Z M 106 20", PATH_ROUNDED_RECT },
  { "M 0 66 L 98 66 L 98 152 L 0 152 Z M 0 66", PATH_RECT },
  { "M 0 0 h 192 v 152 h -192 z", PATH_RECT },
  { "M 93 67"
    "C 87.4765625 67"
    "  83 62.5234375"
    "  83 57"
    "C 83 51.4765625"
    "  87.4765625 47"
    "  93 47"
    "C 98.5234375 47"
    "  103 51.4765625"
    "  103 57"
    "C 103 62.5234375"
    "  98.5234375 67"
    "  93 67"
    "Z"
    "M 94 67", PATH_CIRCLE },
  { "M 52 44.125 C 35.4648438 44.125, 22.0625 57.5273438, 22.0625 74.0625 C 22.0625 90.5976562, 35.4648438 104, 52 104 C 68.5351562 104, 81.9375 90.5976562, 81.9375 74.0625 C 81.9375 57.5273438, 68.5351562 44.125, 52 44.125 Z M 52 44.125", PATH_CIRCLE },
  /* from Adwaitas x-package-repository.svg */
  { "M 64.9921875 74"
    "L 67 74"
    "C 67.5507812 74"
    "  68 74.4492188"
    "  68 75"
    "C 68 75.5507812"
    "  67.5507812 76"
    "  67 76"
    "L 64.9921875 76"
    "C 64.4414062 76"
    "  63.9921875 75.5507812"
    "  63.9921875 75"
    "C 63.9921875 74.4492188"
    "  64.4414062 74"
    "  64.9921875 74"
    "Z"
    "M 64.9921875 74", PATH_ROUNDED_RECT },
  { "M 58.9453125 44 L 69.0546875 44 C 70.6875 44, 72 45.3125, 72 46.9453125 L 72 59.0546875 C 72 60.6875, 70.6875 62, 69.0546875 62 L 58.9453125 62 C 57.3125 62, 56 60.6875, 56 59.0546875 L 56 46.9453125 C 56 45.3125, 57.3125 44, 58.9453125 44 Z M 58.9453125 44", PATH_ROUNDED_RECT },
  /* from panel-right-symbolic.svg */
  { "m 104 502 c 0 0.550781 -0.449219 1 -1 1 s -1 -0.449219 -1 -1 s 0.449219 -1 1 -1 s 1 0.449219 1 1 z m 0 0", PATH_CIRCLE },

  /* from network-wireless-signal-weak-symbolic.svg
   * This is approximating a circle with 5 cubics - too weird to bother with
   */
  { "M 8 10 C 7.48828077 10, 6.97656202 10.1953115, 6.58593798 10.5859385 C 5.80468798 11.3671885, 5.80468798 12.6328115, 6.58593798 13.4140615 C 7.36718798 14.1953115, 8.63281155 14.1953115, 9.41406155 13.4140615 C 10.1953115 12.6328115, 10.1953115 11.3671885, 9.41406155 10.5859385 C 9.02343845 10.1953115, 8.51171875 10, 8 10 Z  M 8 10", PATH_GENERAL },
 /* from org.gnome.SystemMonitor.Devel.svg */
 { "M 113 62 C 113 61.4500008, 113.449997 61, 114 61 C 114.550003 61, 115 61.4500008, 115 62 C 115 62.5499992, 114.550003 63, 114 63 C 113.449997 63, 113 62.5499992, 113 62 Z M 113 62", PATH_CIRCLE },
  /* not from anywhere */
  { "M 69.0546875 44 C 70.6875 44, 72 45.3125, 72 46.9453125 L 72 59.0546875 C 72 60.6875, 70.6875 62, 69.0546875 62 L 58.9453125 62 C 57.3125 62, 56 60.6875, 56 59.0546875 L 56 46.9453125 C 56 45.3125, 57.3125 44, 58.9453125 44 L 58.9453125 44 Z M 58.9453125 44", PATH_ROUNDED_RECT },
  { "M 23.078125 17 L 61.3671875 17 C 62.8007812 17, 63.9609375 18.1601562, 63.9609375 19.5898438 L 63.9609375 56.4101562 C 63.9609375 57.8398438, 62.8007812 59, 61.3671875 59 L 23.078125 59 C 21.6445312 59, 20.484375 57.8398438, 20.484375 56.4101562 L 20.484375 19.5898438 C 20.484375 18.1601562, 21.6445312 17, 23.078125 17 Z M 23.078125 17", PATH_ROUNDED_RECT },
  { "M 66.8554688 42.4921875"
    "L 109.113281 42.4921875"
    "C 113.972656 42.4921875"
    "  117.914062 46.4335938"
    "  117.914062 51.296875"
    "L 117.914062 81.2578125"
    "C 117.914062 86.1210938"
    "  113.972656 90.0625"
    "  109.113281 90.0625"
    "L 66.8554688 90.0625"
    "C 61.9921875 90.0625"
    "  58.0507812 86.1210938"
    "  58.0507812 81.2578125"
    "L 58.0507812 51.296875"
    "C 58.0507812 46.4335938"
    "  61.9921875 42.4921875"
    "  66.8554688 42.4921875"
    "Z"
    "M 66.8554688 42.4921875", PATH_ROUNDED_RECT },
  { "M 87.984375 8.0625"
    "C 104.515625 8.0625"
    "  117.914062 21.4648438"
    "  117.914062 37.9960938"
    "L 117.914062 60.1289062"
    "C 117.914062 76.6601562"
    "  104.515625 90.0625"
    "  87.984375 90.0625"
    "C 71.453125 90.0625"
    "  58.0507812 76.6601562"
    "  58.0507812 60.1289062"
    "L 58.0507812 37.9960938"
    "C 58.0507812 21.4648438"
    "  71.453125 8.0625"
    "  87.984375 8.0625"
    "Z"
    "M 87.984375 8.0625", PATH_ROUNDED_RECT },
  { "M 72.359375 111.140625"
    "L 103.519531 111.140625"
    "C 105.960938 111.140625"
    "  107.9375 113.121094"
    "  107.9375 115.5625"
    "C 107.9375 118.003906"
    "  105.960938 119.984375"
    "  103.519531 119.984375"
    "L 72.359375 119.984375"
    "C 69.9179688 119.984375"
    "  67.9375 118.003906"
    "  67.9375 115.562"
    "C 67.9375 113.121094"
    "  69.9179688 111.140625"
    "  72.359375 111.140625"
    "  Z"
    "M 72.359375 111.140625", PATH_ROUNDED_RECT },
  { "M 20 12"
    "L 108 12"
    "C 112.417969 12"
    "  116 15.7304688"
    "  116 20.3320312"
    "L 116 103.667969"
    "C 116 108.269531"
    "  112.417969 112"
    "  108 112"
    "L 20 112"
    "C 15.5820312 112"
    "  12 108.269531"
    "  12 103.667969"
    "L 12 20.3320312"
    "C 12 15.7304688"
    "  15.5820312 12"
    "  20 12"
    "Z"
    "M 20 12", PATH_ROUNDED_RECT },
};

int
main (int   argc,
      char *argv[])
{
  GskRoundedRect r;
  unsigned int i;

  gtk_test_init (&argc, &argv, NULL);

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    add_test (i + 1, tests[i].path_str, tests[i].expected);

  i++;

  add_rect_test (i++, &GRAPHENE_RECT_INIT (0, 0, 10, 10), PATH_RECT);
  add_rounded_rect_test (i++, &RR (0, 0, 10, 10, 1), PATH_ROUNDED_RECT);
  add_rounded_rect_test (i++, &RR (0, 0, 10, 10, 0), PATH_GENERAL);

  r = GSK_ROUNDED_RECT_INIT (0, 0, 10, 10);
  r.corner[0].width = r.corner[1].height = 1;

  add_rounded_rect_test (i++, &r, PATH_GENERAL);

  return g_test_run ();
}

/* vim:set foldmethod=marker: */

#include <gtk/gtk.h>
#include "gsk/gskcurveprivate.h"

static void
init_random_point (graphene_point_t *p)
{
  p->x = g_test_rand_double_range (0, 1000);
  p->y = g_test_rand_double_range (0, 1000);
}

static float
random_weight (void)
{
  if (g_test_rand_bit ())
    return g_test_rand_double_range (0, 20);
  else
    return 1.0 / g_test_rand_double_range (1, 20);
}

static void
init_random_curve (GskCurve *curve)
{
  switch (g_test_rand_int_range (GSK_PATH_LINE, GSK_PATH_CONIC + 1))
    {
    case GSK_PATH_LINE:
      {
        graphene_point_t p[2];

        init_random_point (&p[0]);
        init_random_point (&p[1]);
        gsk_curve_init (curve, gsk_pathop_encode (GSK_PATH_LINE, p));
      }
      break;

    case GSK_PATH_CURVE:
      {
        graphene_point_t p[4];

        init_random_point (&p[0]);
        init_random_point (&p[1]);
        init_random_point (&p[2]);
        init_random_point (&p[3]);
        gsk_curve_init (curve, gsk_pathop_encode (GSK_PATH_CURVE, p));
      }
    break;

    case GSK_PATH_CONIC:
      {
        graphene_point_t p[4];

        init_random_point (&p[0]);
        init_random_point (&p[1]);
        p[2] = GRAPHENE_POINT_INIT (random_weight(), 0);
        init_random_point (&p[3]);
        gsk_curve_init (curve, gsk_pathop_encode (GSK_PATH_CONIC, p));
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
test_curve_tangents (void)
{
  for (int i = 0; i < 100; i++)
    {
      GskCurve c;
      graphene_vec2_t vec, exact;

      init_random_curve (&c);

      gsk_curve_get_tangent (&c, 0, &vec);
      g_assert_cmpfloat_with_epsilon (graphene_vec2_length (&vec), 1.0f, 0.00001);
      gsk_curve_get_start_tangent (&c, &exact);
      g_assert_cmpfloat_with_epsilon (graphene_vec2_length (&exact), 1.0f, 0.00001);
      g_assert_true (graphene_vec2_near (&vec, &exact, 0.05));

      gsk_curve_get_tangent (&c, 1, &vec);
      g_assert_cmpfloat_with_epsilon (graphene_vec2_length (&vec), 1.0f, 0.00001);
      gsk_curve_get_end_tangent (&c, &exact);
      g_assert_cmpfloat_with_epsilon (graphene_vec2_length (&exact), 1.0f, 0.00001);
      g_assert_true (graphene_vec2_near (&vec, &exact, 0.05));
    }
}

static void
test_curve_points (void)
{
  for (int i = 0; i < 100; i++)
    {
      GskCurve c;
      graphene_point_t p;

      init_random_curve (&c);

      /* We can assert equality here because evaluating the polynomials with 0
       * has no effect on accuracy.
       */
      gsk_curve_get_point (&c, 0, &p);
      g_assert_true (graphene_point_equal (gsk_curve_get_start_point (&c), &p));
      /* But here we evaluate the polynomials with 1 which gives the highest possible
       * accuracy error. So we'll just be generous here.
       */
      gsk_curve_get_point (&c, 1, &p);
      g_assert_true (graphene_point_near (gsk_curve_get_end_point (&c), &p, 0.05));
    }
}

/* at this point the subdivision stops and the decomposer
 * violates tolerance rules
 */
#define MIN_PROGRESS (1/1024.f)

typedef struct
{
  graphene_point_t p;
  float t;
} PointOnLine;

static gboolean
add_line_to_array (const graphene_point_t *from,
                   const graphene_point_t *to,
                   float                   from_progress,
                   float                   to_progress,
                   gpointer                user_data)
{
  GArray *array = user_data;
  PointOnLine *last = &g_array_index (array, PointOnLine, array->len - 1);

  g_assert (array->len > 0);
  g_assert_cmpfloat (from_progress, >=, 0.0f);
  g_assert_cmpfloat (from_progress, <, to_progress);
  g_assert_cmpfloat (to_progress, <=, 1.0f);

  g_assert_true (graphene_point_equal (&last->p, from));
  g_assert_cmpfloat (last->t, ==, from_progress);

  g_array_append_vals (array, (PointOnLine[1]) { { *to, to_progress } }, 1);

  return TRUE;
}

static void
test_curve_decompose (void)
{
  static const float tolerance = 0.5;

  for (int i = 0; i < 100; i++)
    {
      GArray *array;
      GskCurve c;

      init_random_curve (&c);

      array = g_array_new (FALSE, FALSE, sizeof (PointOnLine));
      g_array_append_vals (array, (PointOnLine[1]) { { *gsk_curve_get_start_point (&c), 0.f } }, 1);

      g_assert_true (gsk_curve_decompose (&c, tolerance, add_line_to_array, array));

      g_assert_cmpint (array->len, >=, 2); /* We at least got a line to the end */
      g_assert_cmpfloat (g_array_index (array, PointOnLine, array->len - 1).t, ==, 1.0);

      for (int j = 0; j < array->len; j++)
        {
          PointOnLine *pol = &g_array_index (array, PointOnLine, j);
          graphene_point_t p;

          /* Check that the points we got are actually on the line */
          gsk_curve_get_point (&c, pol->t, &p);
          g_assert_true (graphene_point_near (&pol->p, &p, 0.05));

          /* Check that the mid point is not further than the tolerance */
          if (j > 0)
            {
              PointOnLine *last = &g_array_index (array, PointOnLine, j - 1);
              graphene_point_t mid;

              if (pol->t - last->t > MIN_PROGRESS)
                {
                  graphene_point_interpolate (&last->p, &pol->p, 0.5, &mid);
                  gsk_curve_get_point (&c, (pol->t + last->t) / 2, &p);
                  /* The decomposer does this cheaper Manhattan distance test,
                   * so graphene_point_near() does not work */
                  g_assert_cmpfloat (fabs (mid.x - p.x), <=, tolerance);
                  g_assert_cmpfloat (fabs (mid.y - p.y), <=, tolerance);
                }
            }
        }
    }
}

static void
test_line_line_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[2], p2[2];
  float t1, t2;
  graphene_point_t p;
  int n;

  graphene_point_init (&p1[0], 10, 0);
  graphene_point_init (&p1[1], 10, 100);
  graphene_point_init (&p2[0], 0, 10);
  graphene_point_init (&p2[1], 100, 10);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1, 0.1, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2, 0.1, 0.0001);
  g_assert_true (graphene_point_near (&p, &GRAPHENE_POINT_INIT (10, 10), 0.0001));
}

static void
test_line_curve_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[4], p2[2];
  float t1[9], t2[9];
  graphene_point_t p[9];
  int n;
  graphene_rect_t b;

  graphene_point_init (&p1[0], 0, 100);
  graphene_point_init (&p1[1], 50, 100);
  graphene_point_init (&p1[2], 50, 0);
  graphene_point_init (&p1[3], 100, 0);
  graphene_point_init (&p2[0], 0, 0);
  graphene_point_init (&p2[1], 100, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CURVE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1[0], 0.5, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.5, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (50, 50), 0.0001));

  gsk_curve_get_tight_bounds (&c1, &b);
  graphene_rect_contains_point (&b, &p[0]);

  gsk_curve_get_tight_bounds (&c2, &b);
  graphene_rect_contains_point (&b, &p[0]);
}

static void
test_curve_curve_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[4], p2[4];
  float t1[9], t2[9];
  graphene_point_t p[9];
  int n;
  graphene_rect_t b;

  graphene_point_init (&p1[0], 0, 0);
  graphene_point_init (&p1[1], 33.333, 100);
  graphene_point_init (&p1[2], 66.667, 0);
  graphene_point_init (&p1[3], 100, 100);
  graphene_point_init (&p2[0], 0, 50);
  graphene_point_init (&p2[1], 100, 0);
  graphene_point_init (&p2[2], 20, 0); // weight 20
  graphene_point_init (&p2[3], 50, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CURVE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_CONIC, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 9);

  g_assert_cmpint (n, ==, 2);
  g_assert_cmpfloat (t1[0], <, 0.5);
  g_assert_cmpfloat (t1[1], >, 0.5);
  g_assert_cmpfloat (t2[0], <, 0.5);
  g_assert_cmpfloat (t2[1], >, 0.5);

  gsk_curve_get_tight_bounds (&c1, &b);
  graphene_rect_contains_point (&b, &p[0]);

  gsk_curve_get_tight_bounds (&c2, &b);
  graphene_rect_contains_point (&b, &p[0]);
}

/* This showed up as artifacts in the stroker when our
 * intersection code failed to find intersections with
 * horizontal lines
 */
static void
test_curve_intersection_horizontal_line (void)
{
  GskCurve c1, c2;
  float t1, t2;
  graphene_point_t p;
  int n;

  gsk_curve_init (&c1,
                  gsk_pathop_encode (GSK_PATH_CONIC,
                          (const graphene_point_t[4]) {
                            GRAPHENE_POINT_INIT (200.000, 165.000),
                            GRAPHENE_POINT_INIT (220.858, 165.000),
                            GRAPHENE_POINT_INIT (1.4142, 0),
                            GRAPHENE_POINT_INIT (292.929, 92.929),
                          }));
  gsk_curve_init_foreach (&c2,
                          GSK_PATH_LINE,
                          (const graphene_point_t[2]) {
                            GRAPHENE_POINT_INIT (300, 110),
                            GRAPHENE_POINT_INIT (100, 110),
                          },
                          2,
                          0);

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, 1);

  g_assert_true (n == 1);
}

/* Some sanity checks for splitting curves.
 */
static void
test_curve_split (void)
{
  for (int i = 0; i < 100; i++)
    {
      GskCurve c;
      GskPathBuilder *builder;
      GskPath *path;
      GskPathMeasure *measure;
      const graphene_point_t *s;
      GskCurve c1, c2;
      graphene_point_t p;
      graphene_vec2_t t, t1, t2;

      init_random_curve (&c);

      builder = gsk_path_builder_new ();

      s = gsk_curve_get_start_point (&c);
      gsk_path_builder_move_to (builder, s->x, s->y);
      gsk_curve_builder_to (&c, builder);
      path = gsk_path_builder_free_to_path (builder);
      measure = gsk_path_measure_new_with_tolerance (path, 0.1);

      gsk_curve_split (&c, 0.5, &c1, &c2);

      g_assert_true (c1.op == c.op);
      g_assert_true (c2.op == c.op);

      g_assert_true (graphene_point_near (gsk_curve_get_start_point (&c),
                                          gsk_curve_get_start_point (&c1), 0.005));
      g_assert_true (graphene_point_near (gsk_curve_get_end_point (&c1),
                                          gsk_curve_get_start_point (&c2), 0.005));
      g_assert_true (graphene_point_near (gsk_curve_get_end_point (&c),
                                          gsk_curve_get_end_point (&c2), 0.005));
      gsk_curve_get_point (&c, 0.5, &p);
      gsk_curve_get_tangent (&c, 0.5, &t);
      g_assert_true (graphene_point_near (gsk_curve_get_end_point (&c1), &p, 0.005));
      g_assert_true (graphene_point_near (gsk_curve_get_start_point (&c2), &p, 0.005));

      gsk_curve_get_start_tangent (&c, &t1);
      gsk_curve_get_start_tangent (&c1, &t2);
      g_assert_true (graphene_vec2_near (&t1, &t2, 0.005));
      gsk_curve_get_end_tangent (&c1, &t1);
      gsk_curve_get_start_tangent (&c2, &t2);
      g_assert_true (graphene_vec2_near (&t1, &t2, 0.005));
      g_assert_true (graphene_vec2_near (&t, &t1, 0.005));
      g_assert_true (graphene_vec2_near (&t, &t2, 0.005));
      gsk_curve_get_end_tangent (&c, &t1);
      gsk_curve_get_end_tangent (&c2, &t2);
      g_assert_true (graphene_vec2_near (&t1, &t2, 0.005));

      for (int k = 0; k < 20; k++)
        {
          graphene_point_t q;
          float dist;

          gsk_curve_get_point (&c1, k/19.0, &q);
          dist = gsk_path_measure_get_closest_point (measure, &q, NULL);
          g_assert_cmpfloat (dist, <=, 0.2);

          gsk_curve_get_point (&c2, k/19.0, &q);
          dist = gsk_path_measure_get_closest_point (measure, &q, NULL);
          g_assert_cmpfloat (dist, <=, 0.2);
        }
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/curve/points", test_curve_points);
  g_test_add_func ("/curve/tangents", test_curve_tangents);
  g_test_add_func ("/curve/decompose", test_curve_decompose);
  g_test_add_func ("/curve/intersection/line-line", test_line_line_intersection);
  g_test_add_func ("/curve/intersection/line-curve", test_line_curve_intersection);
  g_test_add_func ("/curve/intersection/curve-curve", test_curve_curve_intersection);
  g_test_add_func ("/curve/intersection/horizontal-line", test_curve_intersection_horizontal_line);
  g_test_add_func ("/curve/split", test_curve_split);

  return g_test_run ();
}

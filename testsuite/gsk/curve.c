#include <gtk/gtk.h>
#include <math.h>
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
    return g_test_rand_double_range (1, 20);
  else
    return 1.0 / g_test_rand_double_range (1, 20);
}

static void
init_random_curve_with_op (GskCurve         *curve,
                           GskPathOperation  min_op,
                           GskPathOperation  max_op)
{
  switch (g_test_rand_int_range (min_op, max_op + 1))
    {
    case GSK_PATH_LINE:
      {
        graphene_point_t p[2];

        init_random_point (&p[0]);
        init_random_point (&p[1]);
        gsk_curve_init (curve, gsk_pathop_encode (GSK_PATH_LINE, p));
      }
      break;

    case GSK_PATH_QUAD:
      {
        graphene_point_t p[3];

        init_random_point (&p[0]);
        init_random_point (&p[1]);
        init_random_point (&p[2]);
        gsk_curve_init (curve, gsk_pathop_encode (GSK_PATH_QUAD, p));
      }
    break;

    case GSK_PATH_CUBIC:
      {
        graphene_point_t p[4];

        init_random_point (&p[0]);
        init_random_point (&p[1]);
        init_random_point (&p[2]);
        init_random_point (&p[3]);
        gsk_curve_init (curve, gsk_pathop_encode (GSK_PATH_CUBIC, p));
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
init_random_curve (GskCurve *curve)
{
  init_random_curve_with_op (curve, GSK_PATH_LINE, GSK_PATH_CONIC);
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

static void
test_curve_bounds (void)
{
  for (int i = 0; i < 100; i++)
    {
      GskCurve c;
      GskBoundingBox bounds;
      GskBoundingBox bounds2;
      graphene_point_t p;

      init_random_curve (&c);

      gsk_curve_get_tight_bounds (&c, &bounds);
      gsk_curve_get_bounds (&c, &bounds2);

      g_assert_true (gsk_bounding_box_contains_point_with_epsilon (&bounds, gsk_curve_get_start_point (&c), 0.001));
      g_assert_true (gsk_bounding_box_contains_point_with_epsilon (&bounds, gsk_curve_get_end_point (&c), 0.001));
      g_assert_true (gsk_bounding_box_contains_point_with_epsilon (&bounds2, gsk_curve_get_start_point (&c), 0.001));
      g_assert_true (gsk_bounding_box_contains_point_with_epsilon (&bounds2, gsk_curve_get_end_point (&c), 0.001));

      for (int j = 0; j < 20; j++)
        {
          float t = g_test_rand_double_range (0, 1);
          gsk_curve_get_point (&c, t, &p);
          g_assert_true (gsk_bounding_box_contains_point_with_epsilon (&bounds, &p, 0.001));
          g_assert_true (gsk_bounding_box_contains_point_with_epsilon (&bounds2, &p, 0.001));
        }
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
                   GskCurveLineReason      reason,
                   gpointer                user_data)
{
  GArray *array = user_data;
  PointOnLine *last = &g_array_index (array, PointOnLine, array->len - 1);

  g_assert_true (array->len > 0);
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

      g_array_unref (array);
    }
}

/* Test that reversing curves of all types produces the
 * expected result
 */
static void
test_curve_reverse (void)
{
  GskCurve c, r;
  graphene_point_t p[4];

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 50, 50);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_LINE, p));

  gsk_curve_reverse (&c, &r);

  g_assert_true (r.op == GSK_PATH_LINE);
  g_assert_true (graphene_point_equal (&r.line.points[0], &p[1]));
  g_assert_true (graphene_point_equal (&r.line.points[1], &p[0]));

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 50, 50);
  graphene_point_init (&p[2], 100, 50);
  graphene_point_init (&p[3], 200, 0);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CUBIC, p));

  gsk_curve_reverse (&c, &r);
  g_assert_true (r.op == GSK_PATH_CUBIC);
  g_assert_true (graphene_point_equal (&r.cubic.points[0], &p[3]));
  g_assert_true (graphene_point_equal (&r.cubic.points[1], &p[2]));
  g_assert_true (graphene_point_equal (&r.cubic.points[2], &p[1]));
  g_assert_true (graphene_point_equal (&r.cubic.points[3], &p[0]));

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 50, 50);
  graphene_point_init (&p[2], 100, 50);
  gsk_curve_init_foreach (&c, GSK_PATH_CONIC, p, 3, 20);

  gsk_curve_reverse (&c, &r);
  g_assert_true (r.op == GSK_PATH_CONIC);
  g_assert_true (r.conic.points[2].x == 20);

  g_assert_true (graphene_point_equal (&r.conic.points[0], &c.conic.points[3]));
  g_assert_true (graphene_point_equal (&r.conic.points[1], &c.conic.points[1]));
  g_assert_true (graphene_point_equal (&r.conic.points[3], &c.conic.points[0]));
}

static gboolean
add_curve_to_array (GskPathOperation        op,
                    const graphene_point_t *pts,
                    gsize                   n_pts,
                    float                   weight,
                    gpointer                user_data)
{
  GArray *array = user_data;
  GskCurve c;

  gsk_curve_init_foreach (&c, op, pts, n_pts, weight);
  g_array_append_val (array, c);

  return TRUE;
}

static void
test_curve_decompose_conic (void)
{
  g_test_skip ("No good error bounds for decomposing conics");
  return;

  for (int i = 0; i < 100; i++)
    {
      GArray *array;
      GskCurve c;
      GskPathBuilder *builder;
      GskPath *path;
      GskPathMeasure *measure;
      const graphene_point_t *s;

      init_random_curve_with_op (&c, GSK_PATH_CONIC, GSK_PATH_CONIC);

      builder = gsk_path_builder_new ();

      s = gsk_curve_get_start_point (&c);
      gsk_path_builder_move_to (builder, s->x, s->y);
      gsk_curve_builder_to (&c, builder);
      path = gsk_path_builder_free_to_path (builder);
      measure = gsk_path_measure_new_with_tolerance (path, 0.1);

      array = g_array_new (FALSE, FALSE, sizeof (GskCurve));

      g_assert_true (gsk_curve_decompose_curve (&c, GSK_PATH_FOREACH_ALLOW_CUBIC, 0.1, add_curve_to_array, array));

      g_assert_cmpint (array->len, >=, 1);

      for (int j = 0; j < array->len; j++)
        {
          GskCurve *c2 = &g_array_index (array, GskCurve, j);

          g_assert_true (c2->op == GSK_PATH_CUBIC);

          /* Check that the curves we got are approximating the conic */
          for (int k = 0; k < 11; k++)
            {
              graphene_point_t p;
              float dist;

              gsk_curve_get_point (c2, k/10.0, &p);
              dist = gsk_path_measure_get_closest_point (measure, &p, NULL);
              g_assert_cmpfloat (dist, <, 0.5); // FIXME error bound ?
            }
        }

      g_array_unref (array);

      gsk_path_measure_unref (measure);
      gsk_path_unref (path);
    }
}

static void
test_curve_decompose_into (GskPathForeachFlags flags)
{
  for (int i = 0; i < 100; i++)
    {
      GskCurve c;
      GskPathBuilder *builder;
      const graphene_point_t *s;
      GskPath *path;
      GArray *array;

      init_random_curve (&c);

      builder = gsk_path_builder_new ();

      s = gsk_curve_get_start_point (&c);
      gsk_path_builder_move_to (builder, s->x, s->y);
      gsk_curve_builder_to (&c, builder);
      path = gsk_path_builder_free_to_path (builder);

      array = g_array_new (FALSE, FALSE, sizeof (GskCurve));

      g_assert_true (gsk_curve_decompose_curve (&c, flags, 0.1, add_curve_to_array, array));

      g_assert_cmpint (array->len, >=, 1);

      for (int j = 0; j < array->len; j++)
        {
          GskCurve *c2 = &g_array_index (array, GskCurve, j);

          switch (c2->op)
            {
            case GSK_PATH_MOVE:
            case GSK_PATH_CLOSE:
            case GSK_PATH_LINE:
              break;
            case GSK_PATH_QUAD:
              g_assert_true (flags & GSK_PATH_FOREACH_ALLOW_QUAD);
              break;
            case GSK_PATH_CUBIC:
              g_assert_true (flags & GSK_PATH_FOREACH_ALLOW_CUBIC);
              break;
            case GSK_PATH_CONIC:
              g_assert_true (flags & GSK_PATH_FOREACH_ALLOW_CONIC);
              break;
            default:
              g_assert_not_reached ();
            }
        }

      g_array_unref (array);
      gsk_path_unref (path);
    }
}

static void
test_curve_decompose_into_line (void)
{
  test_curve_decompose_into (0);
}

static void
test_curve_decompose_into_quad (void)
{
  test_curve_decompose_into (GSK_PATH_FOREACH_ALLOW_QUAD);
}

static void
test_curve_decompose_into_cubic (void)
{
  test_curve_decompose_into (GSK_PATH_FOREACH_ALLOW_CUBIC);
}

static float
angle_between (const graphene_vec2_t *t1,
               const graphene_vec2_t *t2)
{
  float angle = atan2 (graphene_vec2_get_y (t2), graphene_vec2_get_x (t2))
                - atan2 (graphene_vec2_get_y (t1), graphene_vec2_get_x (t1));

  if (angle > M_PI)
    angle -= 2 * M_PI;
  if (angle < - M_PI)
    angle += 2 * M_PI;

  return angle;
}

static float
angle_between_points (const graphene_point_t *c,
                      const graphene_point_t *a,
                      const graphene_point_t *b)
{
  graphene_vec2_t t1, t2;

  graphene_vec2_init (&t1, a->x - c->x, a->y - c->y);
  graphene_vec2_init (&t2, b->x - c->x, b->y - c->y);

  return angle_between (&t1, &t2);
}

#define RAD_TO_DEG(r) ((r)*180.0/M_PI)

static void
test_curve_match (void)
{
  for (int i = 0; i < 100; i++)
    {
      GskCurve c0, c1, c2;
      graphene_point_t p0, p1, p2;
      graphene_vec2_t t0, t1, t2;
      float k0, k1, k2;

      init_random_curve_with_op (&c0, GSK_PATH_QUAD, GSK_PATH_QUAD);
      gsk_curve_raise (&c0, &c1);
      gsk_curve_init_foreach (&c2, GSK_PATH_CONIC, c0.quad.points, 3, 1);

      if (g_test_verbose ())
        {
          g_print ("c0: %s\n", gsk_curve_to_string (&c0));
          g_print ("c1: %s\n", gsk_curve_to_string (&c1));
          g_print ("c2: %s\n", gsk_curve_to_string (&c2));
        }

      p0 = *gsk_curve_get_start_point (&c0);
      p1 = *gsk_curve_get_start_point (&c1);
      p2 = *gsk_curve_get_start_point (&c2);

      g_assert_true (graphene_point_near (&p0, &p1, 0.01));
      g_assert_true (graphene_point_near (&p0, &p2, 0.01));

      p0 = *gsk_curve_get_end_point (&c0);
      p1 = *gsk_curve_get_end_point (&c1);
      p2 = *gsk_curve_get_end_point (&c2);

      g_assert_true (graphene_point_near (&p0, &p1, 0.01));
      g_assert_true (graphene_point_near (&p0, &p2, 0.01));

      gsk_curve_get_start_tangent (&c0, &t0);
      gsk_curve_get_start_tangent (&c1, &t1);
      gsk_curve_get_start_tangent (&c2, &t2);

      g_assert_true (graphene_vec2_near (&t0, &t1, 0.01));
      g_assert_true (graphene_vec2_near (&t0, &t2, 0.01));

      gsk_curve_get_end_tangent (&c0, &t0);
      gsk_curve_get_end_tangent (&c1, &t1);
      gsk_curve_get_end_tangent (&c2, &t2);

      g_assert_true (graphene_vec2_near (&t0, &t1, 0.01));
      g_assert_true (graphene_vec2_near (&t0, &t2, 0.01));

      for (int j = 0; j < 20; j++)
        {
          float t = g_test_rand_double_range (0, 1);

          gsk_curve_get_point (&c0, t, &p0);
          gsk_curve_get_point (&c1, t, &p1);
          gsk_curve_get_point (&c2, t, &p2);

          g_assert_true (graphene_point_near (&p0, &p1, 0.01));
          g_assert_true (graphene_point_near (&p0, &p2, 0.01));

          gsk_curve_get_tangent (&c0, t, &t0);
          gsk_curve_get_tangent (&c1, t, &t1);
          gsk_curve_get_tangent (&c2, t, &t2);

          g_assert_true (graphene_vec2_near (&t0, &t1, 0.01));
          g_assert_true (graphene_vec2_near (&t0, &t2, 0.01));

          if (RAD_TO_DEG (angle_between_points (&c0.quad.points[1], &c0.quad.points[0], &c0.quad.points[2])) < 1)
            {
              /* Skip curvature comparisons for curves that have very sharp turns,
               * since we don't have good absolute error margins there. We should
               * look at relative errors instead.
               */
              continue;
            }

          k0 = gsk_curve_get_curvature (&c0, t, NULL);
          k1 = gsk_curve_get_curvature (&c1, t, NULL);
          k2 = gsk_curve_get_curvature (&c2, t, NULL);

          if (g_test_verbose ())
            {
              g_print ("c0 curvature at %f: %f\n", t, k0);
              g_print ("c1 curvature at %f: %f\n", t, k1);
              g_print ("c2 curvature at %f: %f\n", t, k2);
            }

          g_assert_true (fabs (k0 - k1) < 0.001);
          g_assert_true (fabs (k0 - k2) < 0.001);
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
test_line_line_end_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[2], p2[2];
  float t1, t2;
  graphene_point_t p;
  int n;

  graphene_point_init (&p1[0], 10, 0);
  graphene_point_init (&p1[1], 10, 100);
  graphene_point_init (&p2[0], 10, 100);
  graphene_point_init (&p2[1], 100, 10);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1, 1, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2, 0, 0.0001);
  g_assert_true (graphene_point_near (&p, &GRAPHENE_POINT_INIT (10, 100), 0.0001));
}

static void
test_line_line_none_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[2], p2[2];
  float t1, t2;
  graphene_point_t p;
  int n;

  graphene_point_init (&p1[0], 0, 0);
  graphene_point_init (&p1[1], 10, 0);
  graphene_point_init (&p2[0], 20, 0);
  graphene_point_init (&p2[1], 30, 0);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, 1);

  g_assert_cmpint (n, ==, 0);

  graphene_point_init (&p1[0], 247.103424, 95.7965317);
  graphene_point_init (&p1[1], 205.463974, 266.758484);
  graphene_point_init (&p2[0], 183.735962, 355.968689);
  graphene_point_init (&p2[1], 121.553253, 611.27655);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_LINE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, 1);

  g_assert_cmpint (n, ==, 0);
}

static void
test_line_curve_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[4], p2[2];
  float t1[9], t2[9];
  graphene_point_t p[9];
  int n;
  GskBoundingBox b;

  graphene_point_init (&p1[0], 0, 100);
  graphene_point_init (&p1[1], 50, 100);
  graphene_point_init (&p1[2], 50, 0);
  graphene_point_init (&p1[3], 100, 0);
  graphene_point_init (&p2[0], 0, 0);
  graphene_point_init (&p2[1], 100, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1[0], 0.5, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.5, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (50, 50), 0.0001));

  gsk_curve_get_tight_bounds (&c1, &b);
  gsk_bounding_box_contains_point (&b, &p[0]);

  gsk_curve_get_tight_bounds (&c2, &b);
  gsk_bounding_box_contains_point (&b, &p[0]);
}

static void
test_line_curve_multiple_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[4], p2[2];
  float t1[9], t2[9];
  graphene_point_t p[9];
  graphene_point_t pp;
  int n;
  GskBoundingBox b1, b2;

  graphene_point_init (&p1[0], 100, 200);
  graphene_point_init (&p1[1], 350, 100);
  graphene_point_init (&p1[2], 100, 350);
  graphene_point_init (&p1[3], 400, 300);

  graphene_point_init (&p2[0], 0, 0);
  graphene_point_init (&p2[1], 100, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 3);

  g_assert_cmpint (n, ==, 0);

  graphene_point_init (&p2[0], 0, 0);
  graphene_point_init (&p2[1], 200, 200);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 3);

  g_assert_cmpint (n, ==, 1);

  g_assert_cmpfloat_with_epsilon (t1[0], 0.136196628, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.88487947, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (176.975891, 176.975891), 0.001));

  gsk_curve_get_point (&c1, t1[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  gsk_curve_get_tight_bounds (&c1, &b1);
  gsk_curve_get_tight_bounds (&c2, &b2);

  gsk_bounding_box_contains_point (&b1, &p[0]);
  gsk_bounding_box_contains_point (&b2, &p[0]);

  graphene_point_init (&p2[0], 0, 0);
  graphene_point_init (&p2[1], 280, 280);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 3);

  g_assert_cmpint (n, ==, 2);

  g_assert_cmpfloat_with_epsilon (t1[0], 0.136196628, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.632056773, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (176.975891, 176.975891), 0.001));

  gsk_curve_get_point (&c1, t1[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  g_assert_cmpfloat_with_epsilon (t1[1], 0.499999911, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[1], 0.825892806, 0.0001);
  g_assert_true (graphene_point_near (&p[1], &GRAPHENE_POINT_INIT (231.25, 231.25), 0.001));

  gsk_curve_get_point (&c1, t1[1], &pp);
  g_assert_true (graphene_point_near (&p[1], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[1], &pp);
  g_assert_true (graphene_point_near (&p[1], &pp, 0.001));

  gsk_curve_get_tight_bounds (&c1, &b1);
  gsk_curve_get_tight_bounds (&c2, &b2);

  gsk_bounding_box_contains_point (&b1, &p[0]);
  gsk_bounding_box_contains_point (&b1, &p[1]);
  gsk_bounding_box_contains_point (&b2, &p[0]);
  gsk_bounding_box_contains_point (&b2, &p[1]);

  graphene_point_init (&p2[0], 0, 0);
  graphene_point_init (&p2[1], 1000, 1000);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 3);

  g_assert_cmpint (n, ==, 3);

  g_assert_cmpfloat_with_epsilon (t1[0], 0.863803446, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0.305377066, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (305.377075, 305.377075), 0.001));

  gsk_curve_get_point (&c1, t1[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[0], &pp);
  g_assert_true (graphene_point_near (&p[0], &pp, 0.001));

  g_assert_cmpfloat_with_epsilon (t1[1], 0.136196628, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[1], 0.176975891, 0.0001);
  g_assert_true (graphene_point_near (&p[1], &GRAPHENE_POINT_INIT (176.975891, 176.975891), 0.001));

  gsk_curve_get_point (&c1, t1[1], &pp);
  g_assert_true (graphene_point_near (&p[1], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[1], &pp);
  g_assert_true (graphene_point_near (&p[1], &pp, 0.001));

  g_assert_cmpfloat_with_epsilon (t1[2], 0.5, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[2], 0.231249988, 0.0001);
  g_assert_true (graphene_point_near (&p[2], &GRAPHENE_POINT_INIT (231.249985, 231.249985), 0.001));

  gsk_curve_get_point (&c1, t1[2], &pp);
  g_assert_true (graphene_point_near (&p[2], &pp, 0.001));

  gsk_curve_get_point (&c2, t2[2], &pp);
  g_assert_true (graphene_point_near (&p[2], &pp, 0.001));

  gsk_curve_get_tight_bounds (&c1, &b1);
  gsk_curve_get_tight_bounds (&c2, &b2);

  gsk_bounding_box_contains_point (&b1, &p[0]);
  gsk_bounding_box_contains_point (&b1, &p[1]);
  gsk_bounding_box_contains_point (&b1, &p[2]);
  gsk_bounding_box_contains_point (&b2, &p[0]);
  gsk_bounding_box_contains_point (&b2, &p[1]);
  gsk_bounding_box_contains_point (&b2, &p[2]);
}

static void
test_line_curve_end_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[4], p2[2];
  float t1[9], t2[9];
  graphene_point_t p[9];
  int n;

  graphene_point_init (&p1[0], 0, 100);
  graphene_point_init (&p1[1], 50, 100);
  graphene_point_init (&p1[2], 50, 0);
  graphene_point_init (&p1[3], 100, 0);
  graphene_point_init (&p2[0], 100, 0);
  graphene_point_init (&p2[1], 100, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1[0], 1, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0, 0.0001);
  g_assert_true (graphene_point_near (&p[0], &GRAPHENE_POINT_INIT (100, 0), 0.0001));
}

static void
test_line_curve_none_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[4], p2[2];
  float t1[9], t2[9];
  graphene_point_t p[9];
  int n;

  graphene_point_init (&p1[0], 333, 78);
  graphene_point_init (&p1[1], 415, 78);
  graphene_point_init (&p1[2], 463, 131);
  graphene_point_init (&p1[3], 463, 223);

  graphene_point_init (&p2[0], 520, 476);
  graphene_point_init (&p2[1], 502, 418);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 1);

  g_assert_cmpint (n, ==, 0);
}

static void
test_curve_curve_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[4], p2[4];
  float t1[9], t2[9];
  graphene_point_t p[9];
  int n;
  GskBoundingBox b;

  graphene_point_init (&p1[0], 0, 0);
  graphene_point_init (&p1[1], 33.333, 100);
  graphene_point_init (&p1[2], 66.667, 0);
  graphene_point_init (&p1[3], 100, 100);
  graphene_point_init (&p2[0], 0, 50);
  graphene_point_init (&p2[1], 100, 0);
  graphene_point_init (&p2[2], 20, 0); // weight 20
  graphene_point_init (&p2[3], 50, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_CONIC, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 9);

  g_assert_cmpint (n, ==, 2);
  g_assert_cmpfloat (t1[0], <, 0.5);
  g_assert_cmpfloat (t1[1], >, 0.5);
  g_assert_cmpfloat (t2[0], <, 0.5);
  g_assert_cmpfloat (t2[1], >, 0.5);

  gsk_curve_get_tight_bounds (&c1, &b);
  gsk_bounding_box_contains_point (&b, &p[0]);

  gsk_curve_get_tight_bounds (&c2, &b);
  gsk_bounding_box_contains_point (&b, &p[0]);
}

static void
test_curve_curve_end_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[4], p2[4];
  float t1[9], t2[9];
  graphene_point_t p[9];
  int n;

  graphene_point_init (&p1[0], 0, 0);
  graphene_point_init (&p1[1], 33.333, 100);
  graphene_point_init (&p1[2], 66.667, 0);
  graphene_point_init (&p1[3], 100, 100);
  graphene_point_init (&p2[0], 100, 100);
  graphene_point_init (&p2[1], 100, 0);
  graphene_point_init (&p2[2], 20, 0);
  graphene_point_init (&p2[3], 10, 0);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_CONIC, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 9);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1[0], 1, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2[0], 0, 0.0001);
}

static void
test_curve_curve_end_intersection2 (void)
{
  GskCurve c, c1, c2;
  graphene_point_t p1[4];
  float t1[9], t2[9];
  graphene_point_t p[9];
  int n;

  graphene_point_init (&p1[0], 200, 100);
  graphene_point_init (&p1[1], 300, 300);
  graphene_point_init (&p1[2], 100, 300);
  graphene_point_init (&p1[3], 300, 100);

  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CUBIC, p1));

  gsk_curve_split (&c, 0.5, &c1, &c2);

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 9);

  g_assert_cmpint (n, ==, 2);
}

static void
test_curve_curve_max_intersection (void)
{
  GskCurve c1, c2;
  graphene_point_t p1[4], p2[4];
  float t1[9], t2[9];
  graphene_point_t p[9];
  int n;

  graphene_point_init (&p1[0], 106, 100);
  graphene_point_init (&p1[1], 118, 264);
  graphene_point_init (&p1[2], 129,   4);
  graphene_point_init (&p1[3], 128, 182);

  graphene_point_init (&p2[0],  54, 135);
  graphene_point_init (&p2[1], 263, 136);
  graphene_point_init (&p2[2],   2, 143);
  graphene_point_init (&p2[3], 141, 150);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CUBIC, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_CUBIC, p2));

  n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 9);

  g_assert_cmpint (n, ==, 9);
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

static void
test_curve_intersection_match (void)
{
  for (int i = 0; i < 100; i++)
    {
      GskCurve c0, c1;
      GskCurve d;
      float t10[9], t20[9], t11[9], t21[9];
      graphene_point_t p0[9], p1[9];
      int m0, m1;

      /* We don't compare to the equivalent conic here since our conic
       * intersection code is not up to par.
       */
      init_random_curve_with_op (&c0, GSK_PATH_QUAD, GSK_PATH_QUAD);
      gsk_curve_raise (&c0, &c1);

      init_random_curve_with_op (&d, GSK_PATH_LINE, GSK_PATH_CUBIC);

      if (g_test_verbose ())
        {
          g_print ("q: %s\n", gsk_curve_to_string (&c0));
          g_print ("c: %s\n", gsk_curve_to_string (&c1));
          g_print ("d: %s\n", gsk_curve_to_string (&d));
        }

      m0 = gsk_curve_intersect (&c0, &d, t10, t20, p0, 9);
      m1 = gsk_curve_intersect (&c1, &d, t11, t21, p1, 9);

      g_assert_true (m0 == m1);

      for (int j = 0; j < m0; j++)
        g_assert_true (graphene_point_near (&p0[j], &p1[j], 0.001));
    }
}

/* Some sanity checks for splitting curves. */
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

      init_random_curve_with_op (&c, GSK_PATH_LINE, GSK_PATH_CUBIC);

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
          float dist, tt;

          gsk_curve_get_point (&c1, k/19.0, &q);
          gsk_curve_get_closest_point (&c, &q, &dist, &p, &tt);
          g_print ("%s\nlooking for %g %g (at %g), finding %g %g\n", gsk_curve_to_string (&c), q.x, q.y, k/19.0, p.x, p.y);
          g_assert_cmpfloat (dist, <=, 0.5);

          gsk_curve_get_point (&c2, k/19.0, &q);
          gsk_curve_get_closest_point (&c, &q, &dist, &p, &tt);
          g_print ("looking for %g %g (at %g), finding %g %g\n", q.x, q.y, k/19.0, p.x, p.y);
          g_assert_cmpfloat (dist, <=, 0.5);
        }

      gsk_path_measure_unref (measure);
      gsk_path_unref (path);
    }
}

#define DEG_TO_RAD(x) ((x)*M_PI/180)

static float
line_point_distance (const graphene_point_t *a,
                     const graphene_point_t *b,
                     const graphene_point_t *p)
{
  graphene_vec2_t n, ap, r;

  graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
  graphene_vec2_normalize (&n, &n);

  graphene_vec2_init (&ap, a->x - p->x, a->y - p->y);

  graphene_vec2_scale (&n, graphene_vec2_dot (&ap, &n), &r);

  graphene_vec2_subtract (&ap, &r, &r);

  return graphene_vec2_length (&r);
}

/* Test simple cases of curve offsetting */
static void
test_curve_offset (void)
{
  GskCurve c, r;
  graphene_point_t p[4];

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 50, 0);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_LINE, p));

  gsk_curve_offset (&c, 10, &r);

  g_assert_true (r.op == GSK_PATH_LINE);
  g_assert_true (graphene_point_near (&r.line.points[0], &GRAPHENE_POINT_INIT (0, 10), 0.0001));
  g_assert_true (graphene_point_near (&r.line.points[1], &GRAPHENE_POINT_INIT (50, 10), 0.0001));

  gsk_curve_offset (&c, -10, &r);

  g_assert_true (r.op == GSK_PATH_LINE);
  g_assert_true (graphene_point_near (&r.line.points[0], &GRAPHENE_POINT_INIT (0, -10), 0.0001));
  g_assert_true (graphene_point_near (&r.line.points[1], &GRAPHENE_POINT_INIT (50, -10), 0.0001));

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 50, 0);
  graphene_point_init (&p[2], 100, 50);
  graphene_point_init (&p[3], 100, 100);

  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CUBIC, p));

  gsk_curve_offset (&c, 10, &r);

  g_assert_true (r.op == GSK_PATH_CUBIC);
  g_assert_true (graphene_point_near (&r.cubic.points[0], &GRAPHENE_POINT_INIT (0, 10), 0.0001));

  g_assert_cmpfloat_with_epsilon (r.cubic.points[1].y, 10.0, 0.001);

  g_assert_cmpfloat_with_epsilon (line_point_distance (&p[0], &p[1], &r.cubic.points[1]), 10.0, 0.001);
  g_assert_cmpfloat_with_epsilon (line_point_distance (&p[1], &p[2], &r.cubic.points[1]), 10.0, 0.001);

  g_assert_cmpfloat_with_epsilon (r.cubic.points[2].x, 90.0, 0.001);

  g_assert_cmpfloat_with_epsilon (line_point_distance (&p[1], &p[2], &r.cubic.points[2]), 10.0, 0.001);
  g_assert_cmpfloat_with_epsilon (line_point_distance (&p[2], &p[3], &r.cubic.points[2]), 10.0, 0.001);

  g_assert_true (graphene_point_near (&r.cubic.points[3], &GRAPHENE_POINT_INIT (90, 100), 0.0001));

  gsk_curve_offset (&c, -10, &r);

  g_assert_true (r.op == GSK_PATH_CUBIC);
  g_assert_true (graphene_point_near (&r.cubic.points[0], &GRAPHENE_POINT_INIT (0, -10), 0.0001));

  g_assert_cmpfloat_with_epsilon (r.cubic.points[1].y, -10.0, 0.001);

  g_assert_cmpfloat_with_epsilon (line_point_distance (&p[0], &p[1], &r.cubic.points[1]), 10.0, 0.001);
  g_assert_cmpfloat_with_epsilon (line_point_distance (&p[1], &p[2], &r.cubic.points[1]), 10.0, 0.001);

  g_assert_cmpfloat_with_epsilon (r.cubic.points[2].x, 110.0, 0.001);

  g_assert_cmpfloat_with_epsilon (line_point_distance (&p[1], &p[2], &r.cubic.points[2]), 10.0, 0.001);
  g_assert_cmpfloat_with_epsilon (line_point_distance (&p[2], &p[3], &r.cubic.points[2]), 10.0, 0.001);

  g_assert_true (graphene_point_near (&r.cubic.points[3], &GRAPHENE_POINT_INIT (110, 100), 0.0001));

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 100, 0);
  graphene_point_init (&p[2], 100, 100);

  gsk_curve_init_foreach (&c, GSK_PATH_CONIC, p, 3, 20);

  gsk_curve_offset (&c, 10, &r);

  g_assert_true (r.op == GSK_PATH_CONIC);

  g_assert_true (graphene_point_near (&r.conic.points[0], &GRAPHENE_POINT_INIT (0, 10), 0.0001));
  g_assert_true (graphene_point_near (&r.conic.points[1], &GRAPHENE_POINT_INIT (90, 10), 0.0001));
  g_assert_true (graphene_point_near (&r.conic.points[3], &GRAPHENE_POINT_INIT (90, 100), 0.0001));

  gsk_curve_offset (&c, -10, &r);

  g_assert_true (r.op == GSK_PATH_CONIC);

  g_assert_true (graphene_point_near (&r.conic.points[0], &GRAPHENE_POINT_INIT (0, -10), 0.0001));
  g_assert_true (graphene_point_near (&r.conic.points[1], &GRAPHENE_POINT_INIT (110, -10), 0.0001));
  g_assert_true (graphene_point_near (&r.conic.points[3], &GRAPHENE_POINT_INIT (110, 100), 0.0001));
}

static void
test_curve_closest_point (void)
{
  for (int i = 0; i < 100; i++)
    {
      GskCurve curve;

      init_random_curve_with_op (&curve, GSK_PATH_LINE, GSK_PATH_CUBIC);

      for (int j = 0; j < 100; j++)
        {
          float t = g_test_rand_double_range (0, 1);
          graphene_point_t p, p2;
          float distance, t2;

          g_print ("curve %s\n", gsk_curve_to_string (&curve));
          gsk_curve_get_point (&curve, t, &p);
          g_print ("t %g: %g %g\n", t, p.x, p.y);
          t2  = t;
          gsk_curve_get_closest_point (&curve, &p, &distance, &p2, &t2);

          g_print ("closest %g: %g %g, distance %g\n", t2, p2.x, p2.y, distance);

          g_assert_true (fabs (t2 - t) < 0.0001);
          g_assert_true (graphene_point_near (&p, &p2, 0.1));
        }
    }
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/curve/points", test_curve_points);
  g_test_add_func ("/curve/bounds", test_curve_bounds);
  g_test_add_func ("/curve/tangents", test_curve_tangents);
  g_test_add_func ("/curve/match", test_curve_match);
  g_test_add_func ("/curve/decompose", test_curve_decompose);
  g_test_add_func ("/curve/reverse", test_curve_reverse);
  g_test_add_func ("/curve/decompose/conic", test_curve_decompose_conic);
  g_test_add_func ("/curve/decompose/into/line", test_curve_decompose_into_line);
  g_test_add_func ("/curve/decompose/into/quad", test_curve_decompose_into_quad);
  g_test_add_func ("/curve/decompose/into/cubic", test_curve_decompose_into_cubic);
  g_test_add_func ("/curve/intersection/line-line", test_line_line_intersection);
  g_test_add_func ("/curve/intersection/line-line-none", test_line_line_none_intersection);
  g_test_add_func ("/curve/intersection/line-line-end", test_line_line_end_intersection);
  g_test_add_func ("/curve/intersection/line-curve", test_line_curve_intersection);
  g_test_add_func ("/curve/intersection/line-curve-end", test_line_curve_end_intersection);
  g_test_add_func ("/curve/intersection/line-curve-none", test_line_curve_none_intersection);
  g_test_add_func ("/curve/intersection/line-curve-multiple", test_line_curve_multiple_intersection);
  g_test_add_func ("/curve/intersection/curve-curve", test_curve_curve_intersection);
  g_test_add_func ("/curve/intersection/curve-curve-end", test_curve_curve_end_intersection);
  g_test_add_func ("/curve/intersection/curve-curve-end2", test_curve_curve_end_intersection2);
  g_test_add_func ("/curve/intersection/curve-curve-max", test_curve_curve_max_intersection);
  g_test_add_func ("/curve/intersection/horizontal-line", test_curve_intersection_horizontal_line);
  g_test_add_func ("/curve/intersection/match", test_curve_intersection_match);
  g_test_add_func ("/curve/split", test_curve_split);
  g_test_add_func ("/curve/offset", test_curve_offset);
  g_test_add_func ("/curve/closest-point", test_curve_closest_point);

  return g_test_run ();
}

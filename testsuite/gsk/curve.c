#include <gtk/gtk.h>
#include "gsk/gskcurveprivate.h"

static void
init_random_point (graphene_point_t *p)
{
  p->x = g_test_rand_double_range (0, 1000);
  p->y = g_test_rand_double_range (0, 1000);
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
        p[2] = GRAPHENE_POINT_INIT (g_test_rand_double_range (0, 20), 0);
        init_random_point (&p[3]);
        gsk_curve_init (curve, gsk_pathop_encode (GSK_PATH_CONIC, p));
      }
      break;

    default:
      g_assert_not_reached ();
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

      gsk_curve_eval (&c, 0, &p, NULL);
      g_assert_true (graphene_point_near (gsk_curve_get_start_point (&c), &p, 0.01));
      gsk_curve_eval (&c, 1, &p, NULL);
      g_assert_true (graphene_point_near (gsk_curve_get_end_point (&c), &p, 0.01));
    }
}

static void
test_curve_tangents (void)
{
  GskCurve c;
  graphene_point_t p[4];
  graphene_vec2_t t;

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 100, 0);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_LINE, p));

  gsk_curve_get_start_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));
  gsk_curve_get_end_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));


  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 0, 100);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_LINE, p));

  gsk_curve_get_start_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_y_axis (), 0.0001));
  gsk_curve_get_end_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_y_axis (), 0.0001));

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 100, 0);
  p[2] = GRAPHENE_POINT_INIT (g_test_rand_double_range (0, 20), 0);
  graphene_point_init (&p[3], 100, 100);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CONIC, p));

  gsk_curve_get_start_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));
  gsk_curve_get_end_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_y_axis (), 0.0001));

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 50, 0);
  graphene_point_init (&p[2], 100, 50);
  graphene_point_init (&p[3], 100, 100);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CURVE, p));

  gsk_curve_get_start_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));
  gsk_curve_get_end_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_y_axis (), 0.0001));
}

static void
test_curve_bounds (void)
{
  for (int i = 0; i < 100; i++)
    {
      GskCurve c;
      graphene_rect_t bounds;
      graphene_rect_t hull;

      init_random_curve (&c);
      gsk_curve_get_bounds (&c, &hull);
      gsk_curve_get_tight_bounds (&c, &bounds);

      graphene_rect_inset (&hull, - 0.5, - 0.5); // FIXME: this seems big
      g_assert_true (graphene_rect_contains_rect (&hull, &bounds));
      graphene_rect_inset (&bounds, - 0.5, - 0.5);

      for (int j = 0; j < 100; j++)
        {
          graphene_point_t p;

          gsk_curve_eval (&c, j / 99.0, &p, NULL);
          g_assert_true (graphene_rect_contains_point (&bounds, &p));
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
  float t1, t2;
  graphene_point_t p;
  int n;

  graphene_point_init (&p1[0], 0, 100);
  graphene_point_init (&p1[1], 50, 100);
  graphene_point_init (&p1[2], 50, 0);
  graphene_point_init (&p1[3], 100, 0);
  graphene_point_init (&p2[0], 0, 0);
  graphene_point_init (&p2[1], 100, 100);

  gsk_curve_init (&c1, gsk_pathop_encode (GSK_PATH_CURVE, p1));
  gsk_curve_init (&c2, gsk_pathop_encode (GSK_PATH_LINE, p2));

  n = gsk_curve_intersect (&c1, &c2, &t1, &t2, &p, 1);

  g_assert_cmpint (n, ==, 1);
  g_assert_cmpfloat_with_epsilon (t1, 0.5, 0.0001);
  g_assert_cmpfloat_with_epsilon (t2, 0.5, 0.0001);
  g_assert_true (graphene_point_near (&p, &GRAPHENE_POINT_INIT (50, 50), 0.0001));
}

static void
test_curve_curve_intersection (void)
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
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/curve/points", test_curve_points);
  g_test_add_func ("/curve/tangents", test_curve_tangents);
  g_test_add_func ("/curve/bounds", test_curve_bounds);
  g_test_add_func ("/curve/intersection/line-line", test_line_line_intersection);
  g_test_add_func ("/curve/intersection/line-curve", test_line_curve_intersection);
  g_test_add_func ("/curve/intersection/curve-curve", test_curve_curve_intersection);

  return g_test_run ();
}

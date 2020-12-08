#include <gtk/gtk.h>
#include "gsk/gskcurveprivate.h"

static void
init_random_point (graphene_point_t *p)
{
  p->x = g_test_rand_double_range (0, 1000);
  p->y = g_test_rand_double_range (0, 1000);
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
test_intersection (GskPathOperation op1,
                   GskPathOperation op2)
{
  GskCurve curves[10000];
  GskCurve *c1, *c2;
  gint64 start, time;
  gint64 count;
  double time_elapsed;
  double result;
  float t1[9], t2[9];
  graphene_point_t p[9];

  for (int i = 0; i < 5000; i++)
    {
      init_random_curve_with_op (&curves[i], op1, op1);
      init_random_curve_with_op (&curves[5000 + i], op2, op2);
    }

  count = 0;
  start = time = g_get_monotonic_time ();

  while (time - start < G_TIME_SPAN_SECOND * 20)
    {
      c1 = &curves[g_test_rand_int_range (0, 5000)];
      c2 = &curves[g_test_rand_int_range (5000, 10000)];

      gsk_curve_intersect (c1, c2, t1, t2, p, 9);

      count++;
      if (count % 10000 == 0)
        time = g_get_monotonic_time ();
    }

  time_elapsed = (time - start) / (double) G_TIME_SPAN_SECOND;
  result = ((double) count / time_elapsed);
  g_test_maximized_result (result, "%8.1f ops/s", result);
}

static void
line_intersection (void)
{
  test_intersection (GSK_PATH_LINE, GSK_PATH_LINE);
}

static void
line_curve_intersection (void)
{
  test_intersection (GSK_PATH_LINE, GSK_PATH_CURVE);
}

static void
curve_intersection (void)
{
  test_intersection (GSK_PATH_CURVE, GSK_PATH_CURVE);
}

static void
conic_intersection (void)
{
  test_intersection (GSK_PATH_CONIC, GSK_PATH_CONIC);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  if (g_test_perf ())
    {
      g_test_add_func ("/curve/perf/intersection/line", line_intersection);
      g_test_add_func ("/curve/perf/intersection/line-curve", line_curve_intersection);
      g_test_add_func ("/curve/perf/intersection/curve", curve_intersection);
      g_test_add_func ("/curve/perf/intersection/conic", conic_intersection);
    }

  return g_test_run ();
}


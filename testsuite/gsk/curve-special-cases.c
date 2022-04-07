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

#include <gtk/gtk.h>

#include "gsk/gskcurveprivate.h"

static gboolean
measure_segment (const graphene_point_t *from,
                 const graphene_point_t *to,
                 float                   from_t,
                 float                   to_t,
                 GskCurveLineReason      reason,
                 gpointer                data)
{
  float *length = data;

  *length += graphene_point_distance (from, to, NULL, NULL);

  return TRUE;
}

static float
measure_length (const GskCurve *curve)
{
  float result = 0;

  gsk_curve_decompose (curve, 0.5, measure_segment, &result);

  return result;
}

/* This is a pretty nasty conic that makes it obvious that split()
 * does not respect the progress values, so split() twice  with
 * scaled factor won't work.
 */
static void
test_conic_segment (void)
{
  GskCurve c, s, e, m;
  graphene_point_t pts[4] = {
    GRAPHENE_POINT_INIT (-1856.131591796875, 46.217609405517578125),
    GRAPHENE_POINT_INIT (-1555.9866943359375, 966.0810546875),
    GRAPHENE_POINT_INIT (98.94945526123046875, 0),
    GRAPHENE_POINT_INIT (-1471.33154296875, 526.701171875)
  };
  float start = 0.02222645096480846405029296875;
  float end = 0.982032716274261474609375;

  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CONIC, pts));

  gsk_curve_split (&c, start, &s, NULL);
  gsk_curve_segment (&c, start, end, &m);
  gsk_curve_split (&c, end, NULL, &e);

  g_assert_cmpfloat_with_epsilon (measure_length (&c), measure_length (&s) + measure_length (&m) + measure_length (&e), 0.03125);
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
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CUBIC, p));

  gsk_curve_get_start_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));
  gsk_curve_get_end_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_y_axis (), 0.0001));
}

static gboolean
near_one_point (const graphene_point_t *p,
                const graphene_point_t *q,
                int                     n,
                float                   epsilon)
{
  for (int i = 0; i < n; i++)
    {
      if (graphene_point_near (p, &q[i], epsilon))
        return TRUE;
    }

  return FALSE;
}

static gboolean
pathop_cb (GskPathOperation        op,
           const graphene_point_t *pts,
           gsize                   n_pts,
           float                   weight,
           gpointer                user_data)
{
  GskCurve *curve = user_data;

  g_assert (op != GSK_PATH_CLOSE);

  if (op == GSK_PATH_MOVE)
    return TRUE;

  gsk_curve_init_foreach (curve, op, pts, n_pts, weight);
  return FALSE;
}

static void
parse_curve (GskCurve   *c,
             const char *str)
{
  GskPath *path = gsk_path_parse (str);

  gsk_path_foreach (path, GSK_PATH_FOREACH_ALLOW_CUBIC | GSK_PATH_FOREACH_ALLOW_CONIC, pathop_cb, c);

  gsk_path_unref (path);
}

static void
test_curve_intersections (void)
{
  struct {
    const char *c1;
    const char *c2;
    int n;
    graphene_point_t p[9];
  } tests[] = {
    { "M 0 100 L 100 100",
      "M 0 110 L 100 110",
      0, { { 0, 0 }, },
    },
    { "M 0 100 L 100 100",
      "M 110 100 L 210 100",
      0, { { 0, 0 }, },
    },
    { "M 0 100 L 100 100",
      "M 0 100 L -100 100",
      1, { { 0, 100 }, },
    },
    { "M 0 100 L 100 100",
      "M 20 100 L 80 100",
      2, { { 20, 100 }, { 80, 100 }, },
    },
    { "M 0 100 L 100 100",
      "M 150 100 L 50 100",
      2, { { 100, 100 }, { 50, 100 }, },
    },
    { "M 888 482 C 999.333313 508.666687 1080.83325 544.333313 1132.5 589",
      "M 886 680 L 642 618",
      0, { { 0, 0 }, },
    },
    { "M 1119.5 772 C 1039.16675 850.666687 925.333313 890 778 890",
      "M 1052 1430 734 762",
      1, { { 794.851257, 889.825439 }, },
    },
    { "M 844.085 271.845 Q 985.723 94.0499 836.718 817.477",
      "M 790.206 34.4028 L 965.236 893.041",
      1,
      { { 890.685, 527.323 }, }
    },
    { "M 521.412 466.917 Q 838.809 472.131 51.3819 192.985",
      "M 854.519 682.333 Q 154.655 -50.3073 260.046 627.56",
      3, { { 611.932129, 450.019135 }, { 518.727844, 377.701019 }, { 343.737976, 301.792297 } }
    },
    { "M 521.412 466.917 Q 838.809 472.131 51.3819 192.985",
      "M 854.519 682.333 O 154.655 -50.3073 260.046 627.56 1",
      3, { { 611.932129, 450.019135 }, { 518.727844, 377.701019 }, { 343.737976, 301.792297 } }
    },
    { "M 521.412 466.917 Q 838.809 472.131 51.3819 192.985",
      "M 854.519 682.333 O 154.655 50.3073 260.046 627.56 1.53362",
      3, { { 597.725, 460.362 }, { 426.752, 335.879 }, { 310.528, 288.720 }, },
    },
  };

  for (unsigned int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      GskCurve c1, c2;
      float t1[9], t2[9];
      graphene_point_t p[9];
      int n;

      parse_curve (&c1, tests[i].c1);
      parse_curve (&c2, tests[i].c2);

      n = gsk_curve_intersect (&c1, &c2, t1, t2, p, 9);

      if (g_test_verbose ())
        g_print ("expected %d intersections, got %d\n", tests[i].n, n);

      if (c1.op == GSK_PATH_CONIC || c2.op == GSK_PATH_CONIC)
        {
          /* Our conic intersection code can produce duplicate intersections */

          for (unsigned int j = 0; j < tests[i].n; j++)
            {
              if (g_test_verbose ())
                g_print ("looking for %f %f\n", tests[i].p[j].x, tests[i].p[j].y);
              g_assert_true (near_one_point (&tests[i].p[j], p, n, 0.01));
              if (g_test_verbose ())
                g_print ("found expected intersection %d\n", j);
            }
          for (unsigned int j = 0; j < n; j++)
            {
              if (g_test_verbose ())
                g_print ("looking for %f %f\n", p[j].x, p[j].y);
              g_assert_true (near_one_point (&p[j], tests[i].p, tests[i].n, 0.01));
              if (g_test_verbose ())
                g_print ("intersection %d is expected\n", j);
            }
        }
      else
        {
          g_assert_true (n == tests[i].n);

          for (unsigned int j = 0; j < n; j++)
            {
              if (g_test_verbose ())
                g_print ("expected %f %f got %f %f\n",
                         tests[i].p[j].x, tests[i].p[j].y,
                         p[j].x, p[j].y);
              g_assert_true (graphene_point_near (&p[j], &tests[i].p[j], 0.001));
              if (g_test_verbose ())
                g_test_message ("intersection %d OK", j);
            }
        }

      if (g_test_verbose ())
        g_test_message ("test %d OK", i);
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/curve/special/conic-segment", test_conic_segment);
  g_test_add_func ("/curve/special/tangents", test_curve_tangents);
  g_test_add_func ("/curve/special/intersections", test_curve_intersections);

  return g_test_run ();
}

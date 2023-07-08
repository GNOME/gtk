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
  graphene_point_init (&p[1], 50, 0);
  graphene_point_init (&p[2], 100, 50);
  graphene_point_init (&p[3], 100, 100);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CUBIC, p));

  gsk_curve_get_start_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));
  gsk_curve_get_end_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_y_axis (), 0.0001));
}

static void
test_curve_degenerate_tangents (void)
{
  GskCurve c;
  graphene_point_t p[4];
  graphene_vec2_t t;

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 0, 0);
  graphene_point_init (&p[2], 100, 0);
  graphene_point_init (&p[3], 100, 0);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CUBIC, p));

  gsk_curve_get_start_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));
  gsk_curve_get_end_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));

  graphene_point_init (&p[0], 0, 0);
  graphene_point_init (&p[1], 50, 0);
  graphene_point_init (&p[2], 50, 0);
  graphene_point_init (&p[3], 100, 0);
  gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CUBIC, p));

  gsk_curve_get_start_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));
  gsk_curve_get_end_tangent (&c, &t);
  g_assert_true (graphene_vec2_near (&t, graphene_vec2_x_axis (), 0.0001));
}

static gboolean
pathop_cb (GskPathOperation        op,
           const graphene_point_t *pts,
           gsize                   n_pts,
           gpointer                user_data)
{
  GskCurve *curve = user_data;

  g_assert (op != GSK_PATH_CLOSE);

  if (op == GSK_PATH_MOVE)
    return TRUE;

  gsk_curve_init_foreach (curve, op, pts, n_pts);
  return FALSE;
}

static void
parse_curve (GskCurve   *c,
             const char *str)
{
  GskPath *path = gsk_path_parse (str);

  gsk_path_foreach (path, -1, pathop_cb, c);

  gsk_path_unref (path);
}

static void
test_curve_crossing (void)
{
  struct {
    const char *c;
    const graphene_point_t p;
    int crossing;
  } tests[] = {
    { "M 0 0 L 200 200", { 200, 100 }, 0 },
    { "M 0 0 L 200 200", { 0, 100 }, 1 },
    { "M 0 200 L 200 0", { 0, 100 }, -1 },
    { "M 0 0 C 100 100 200 200 300 300", { 200, 100 }, 0 },
    { "M 0 0 C 100 100 200 200 300 300", { 0, 100 }, 1 },
    { "M 0 300 C 100 200 200 100 300 0", { 0, 100 }, -1 },
    { "M 0 0 C 100 600 200 -300 300 300", { 0, 150 }, 1 },
    { "M 0 0 C 100 600 200 -300 300 300", { 100, 150 }, 0 },
    { "M 0 0 C 100 600 200 -300 300 300", { 200, 150 }, 1 },
  };

  for (unsigned int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      GskCurve c;

      parse_curve (&c, tests[i].c);

      g_assert_true (gsk_curve_get_crossing (&c, &tests[i].p) == tests[i].crossing);
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/curve/special/tangents", test_curve_tangents);
  g_test_add_func ("/curve/special/degenerate-tangents", test_curve_degenerate_tangents);
  g_test_add_func ("/curve/special/crossing", test_curve_crossing);

  return g_test_run ();
}

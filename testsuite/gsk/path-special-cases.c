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

/* testcases from path_parser.rs in librsvg */
static void
test_rsvg_parse (void)
{
  struct {
    const char *in;
    const char *out;
  } tests[] = {
    { "", "" },
    // numbers
    { "M 10 20", "M 10 20" },
    { "M -10 -20", "M -10 -20" },
    { "M .10 0.20", "M 0.1 0.2" },
    { "M -.10 -0.20", "M -0.1 -0.2" },
    { "M-.10-0.20", "M -0.1 -0.2" },
    { "M10.5.50", "M 10.5 0.5" },
    { "M.10.20", "M 0.1 0.2" },
    { "M .10E1 .20e-4", "M 1 2e-05" },
    { "M-.10E1-.20", "M -1 -0.2" },
    { "M10.10E2 -0.20e3", "M 1010 -200" },
    { "M-10.10E2-0.20e-3", "M -1010 -0.0002" },
    { "M1e2.5", "M 100 0.5" },
    { "M1e-2.5", "M 0.01 0.5" },
    { "M1e+2.5", "M 100 0.5" },
    // bogus numbers
    { "M+", NULL },
    { "M-", NULL },
    { "M+x", NULL },
    { "M10e", NULL },
    { "M10ex", NULL },
    { "M10e-", NULL },
    { "M10e+x", NULL },
    // numbers with comma
    { "M 10, 20", "M 10 20" },
    { "M -10,-20", "M -10 -20" },
    { "M.10    ,     0.20", "M 0.1 0.2" },
    { "M -.10, -0.20   ", "M -0.1 -0.2" },
    { "M-.10-0.20", "M -0.1 -0.2" },
    { "M.10.20", "M 0.1 0.2" },
    { "M .10E1,.20e-4", "M 1 2e-05" },
    { "M-.10E-2,-.20", "M -0.001 -0.2" },
    { "M10.10E2,-0.20e3", "M 1010 -200" },
    { "M-10.10E2,-0.20e-3", "M -1010 -0.0002" },
    // single moveto
    { "M 10 20 ", "M 10 20" },
    { "M10,20  ", "M 10 20" },
    { "M10 20   ", "M 10 20" },
    { "    M10,20     ", "M 10 20" },
    // relative moveto
    { "m10 20", "M 10 20" },
    // absolute moveto with implicit lineto
    { "M10 20 30 40", "M 10 20 L 30 40" },
    { "M10,20,30,40", "M 10 20 L 30 40" },
    { "M.1-2,3E2-4", "M 0.1 -2 L 300 -4" },
    // relative moveto with implicit lineto
    { "m10 20 30 40", "M 10 20 L 40 60" },
    // relative moveto with relative lineto sequence
    { "m 46,447 l 0,0.5 -1,0 -1,0 0,1 0,12",
      "M 46 447 L 46 447.5 L 45 447.5 L 44 447.5 L 44 448.5 L 44 460.5" },
    // absolute moveto with implicit linetos
    { "M10,20 30,40,50 60", "M 10 20 L 30 40 L 50 60" },
    // relative moveto with implicit linetos
    { "m10 20 30 40 50 60", "M 10 20 L 40 60 L 90 120" },
    // absolute moveto moveto
    { "M10 20 M 30 40", "M 10 20 M 30 40" },
    // relative moveto moveto
    { "m10 20 m 30 40", "M 10 20 M 40 60" },
    // relative moveto lineto moveto
    { "m10 20 30 40 m 50 60", "M 10 20 L 40 60 M 90 120" },
    // absolute moveto lineto
    { "M10 20 L30,40", "M 10 20 L 30 40" },
    // relative moveto lineto
    { "m10 20 l30,40", "M 10 20 L 40 60" },
    // relative moveto lineto lineto abs lineto
    { "m10 20 30 40l30,40,50 60L200,300",
      "M 10 20 L 40 60 L 70 100 L 120 160 L 200 300" },
    // horizontal lineto
    { "M10 20 H30", "M 10 20 L 30 20" },
    { "M 10 20 H 30 40",  "M 10 20 L 30 20 L 40 20" },
    { "M10 20 H30,40-50", "M 10 20 L 30 20 L 40 20 L -50 20" },
    { "m10 20 h30,40-50", "M 10 20 L 40 20 L 80 20 L 30 20" },
    // vertical lineto
    { "M10 20 V30", "M 10 20 L 10 30" },
    { "M10 20 V30 40", "M 10 20 L 10 30 L 10 40" },
    { "M10 20 V30,40-50", "M 10 20 L 10 30 L 10 40 L 10 -50" },
    { "m10 20 v30,40-50", "M 10 20 L 10 50 L 10 90 L 10 40" },
    // curveto
    { "M10 20 C 30,40 50 60-70,80", "M 10 20 C 30 40, 50 60, -70 80" },
    { "M10 20 C 30,40 50 60-70,80,90 100,110 120,130,140",
      "M 10 20 C 30 40, 50 60, -70 80 C 90 100, 110 120, 130 140" },
    { "m10 20 c 30,40 50 60-70,80,90 100,110 120,130,140",
      "M 10 20 C 40 60, 60 80, -60 100 C 30 200, 50 220, 70 240" },
    { "m10 20 c 30,40 50 60-70,80 90 100,110 120,130,140",
      "M 10 20 C 40 60, 60 80, -60 100 C 30 200, 50 220, 70 240" },
    // smooth curveto
    { "M10 20 S 30,40-50,60", "M 10 20 C 10 20, 30 40, -50 60" },
    { "M10 20 S 30,40 50 60-70,80,90 100",
      "M 10 20 C 10 20, 30 40, 50 60 C 70 80, -70 80, 90 100" },
    // quadratic curveto
    { "M10 20 Q30 40 50 60", "M 10 20 Q 30 40, 50 60" },
    { "M10 20 Q30 40 50 60,70,80-90 100",
      "M 10 20 Q 30 40, 50 60 Q 70 80, -90 100" },
    { "m10 20 q 30,40 50 60-70,80 90 100",
      "M 10 20 Q 40 60, 60 80 Q -10 160, 150 180" },
    // smooth quadratic curveto
    { "M10 20 T30 40", "M 10 20 Q 10 20, 30 40" },
    { "M10 20 Q30 40 50 60 T70 80", "M 10 20 Q 30 40, 50 60 Q 70 80, 70 80" },
    { "m10 20 q 30,40 50 60t-70,80",
      "M 10 20 Q 40 60, 60 80 Q 80 100, -10 160" },
    // elliptical arc. Exact numbers depend on too much math, so just verify
    // that these parse successfully
    { "M 1 3 A 1 2 3 00 6 7", "path" },
    { "M 1 2 A 1 2 3 016 7", "path" },
    { "M 1 2 A 1 2 3 10,6 7", "path" },
    { "M 1 2 A 1 2 3 1,1 6 7", "path" },
    { "M 1 2 A 1 2 3 1 1 6 7", "path" },
    { "M 1 2 A 1 2 3 1 16 7", "path" },
    // close path
    { "M10 20 Z", "M 10 20 Z" },
    { "m10 20 30 40 m 50 60 70 80 90 100z", "M 10 20 L 40 60 M 90 120 L 160 200 L 250 300 Z" },
    // must start with moveto
    { " L10 20", NULL },
    // moveto args
    { "M", NULL },
    { "M,", NULL },
    { "M10", NULL },
    { "M10,", NULL },
    { "M10x", NULL },
    { "M10,x", NULL },
    { "M10-20,", NULL },
    { "M10-20-30", NULL },
    { "M10-20-30 x", NULL },
    // closepath args
    { "M10-20z10", NULL },
    { "M10-20z,", NULL },
    // lineto args
    { "M10-20L10", NULL },
    { "M 10,10 L 20,20,30", NULL },
    { "M 10,10 L 20,20,", NULL },
    // horizontal lineto args
    { "M10-20H", NULL },
    { "M10-20H,", NULL },
    { "M10-20H30,", NULL },
    // vertical lineto args
    { "M10-20v", NULL },
    { "M10-20v,", NULL },
    { "M10-20v30,", NULL },
    // curveto args
    { "M10-20C1", NULL },
    { "M10-20C1,", NULL },
    { "M10-20C1 2", NULL },
    { "M10-20C1,2,", NULL },
    { "M10-20C1 2 3", NULL },
    { "M10-20C1,2,3", NULL },
    { "M10-20C1,2,3,", NULL },
    { "M10-20C1 2 3 4", NULL },
    { "M10-20C1,2,3,4", NULL },
    { "M10-20C1,2,3,4,", NULL },
    { "M10-20C1 2 3 4 5", NULL },
    { "M10-20C1,2,3,4,5", NULL },
    { "M10-20C1,2,3,4,5,", NULL },
    { "M10-20C1,2,3,4,5,6,", NULL },
    // smooth curveto args
    { "M10-20S1", NULL },
    { "M10-20S1,", NULL },
    { "M10-20S1 2", NULL },
    { "M10-20S1,2,", NULL },
    { "M10-20S1 2 3", NULL },
    { "M10-20S1,2,3,", NULL },
    { "M10-20S1,2,3,4,", NULL },
    // quadratic curveto args
    { "M10-20Q1", NULL },
    { "M10-20Q1,", NULL },
    { "M10-20Q1 2", NULL },
    { "M10-20Q1,2,", NULL },
    { "M10-20Q1 2 3", NULL },
    { "M10-20Q1,2,3", NULL },
    { "M10-20Q1,2,3,", NULL },
    { "M10 20 Q30 40 50 60,", NULL },
    // smooth quadratic curveto args
    { "M10-20T1", NULL },
    { "M10-20T1,", NULL },
    { "M10 20 T 30 40,", NULL },
    // elliptical arc args
    { "M10-20A1", NULL },
    { "M10-20A1,", NULL },
    { "M10-20A1 2", NULL },
    { "M10-20A1 2,", NULL },
    { "M10-20A1 2 3", NULL },
    { "M10-20A1 2 3,", NULL },
    { "M10-20A1 2 3 4", NULL },
    { "M10-20A1 2 3 1", NULL },
    { "M10-20A1 2 3,1,", NULL },
    { "M10-20A1 2 3 1 5", NULL },
    { "M10-20A1 2 3 1 1", NULL },
    { "M10-20A1 2 3,1,1,", NULL },
    { "M10-20A1 2 3 1 1 6", NULL },
    { "M10-20A1 2 3,1,1,6,", NULL },
    { "M 1 2 A 1 2 3 1.0 0.0 6 7", NULL },
    { "M10-20A1 2 3,1,1,6,7,", NULL },
    // misc
    { "M.. 1,0 0,100000", NULL },
    { "M 10 20,M 10 20", NULL },
    { "M 10 20, M 10 20", NULL },
    { "M 10 20, M 10 20", NULL },
    { "M 10 20, ", NULL },
  };
  int i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      GskPath *path;
      char *string;
      char *string2;

      if (g_test_verbose ())
        g_print ("%d: %s\n", i, tests[i].in);

      path = gsk_path_parse (tests[i].in);
      if (tests[i].out)
        {
          g_assert_nonnull (path);
          string = gsk_path_to_string (path);
          gsk_path_unref (path);

          if (strcmp (tests[i].out, "path") != 0)
            {
              /* Preferred, but doesn't work, because gsk_path_print()
               * prints numbers with insane accuracy
               */
              /* g_assert_cmpstr (tests[i].out, ==, string); */
              path = gsk_path_parse (tests[i].out);
              g_assert_nonnull (path);

              string2 = gsk_path_to_string (path);
              gsk_path_unref (path);

              g_assert_cmpstr (string, ==, string2);

              g_free (string2);
            }

          path = gsk_path_parse (string);
          g_assert_nonnull (path);

          string2 = gsk_path_to_string (path);
          gsk_path_unref (path);

          g_assert_cmpstr (string, ==, string2);

          g_free (string);
          g_free (string2);
        }
      else
        g_assert_null (path);
    }
}

static void
test_empty_path (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  char *s;
  graphene_rect_t bounds;
  GskPathPoint point;

  builder = gsk_path_builder_new ();
  path = gsk_path_builder_free_to_path (builder);

  g_assert_true (gsk_path_is_empty (path));
  g_assert_false (gsk_path_is_closed (path));

  s = gsk_path_to_string (path);
  g_assert_cmpstr (s, ==, "");
  g_free (s);

  g_assert_false (gsk_path_get_bounds (path, &bounds));
  g_assert_true (graphene_rect_equal (&bounds, graphene_rect_zero ()));

  g_assert_false (gsk_path_in_fill (path, &GRAPHENE_POINT_INIT (0, 0), GSK_FILL_RULE_WINDING));

  g_assert_false (gsk_path_get_closest_point (path, &GRAPHENE_POINT_INIT (0, 0), INFINITY, &point));

  gsk_path_unref (path);
}

static void
test_rect_path (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  char *s;
  graphene_rect_t bounds;
  GskPathPoint point;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (0, 0, 200, 100));
  path = gsk_path_builder_free_to_path (builder);

  g_assert_false (gsk_path_is_empty (path));
  g_assert_true (gsk_path_is_closed (path));

  s = gsk_path_to_string (path);
  g_assert_cmpstr (s, ==, "M 0 0 L 200 0 L 200 100 L 0 100 Z");
  g_free (s);

  g_assert_true (gsk_path_get_bounds (path, &bounds));
  g_assert_true (graphene_rect_equal (&bounds, &GRAPHENE_RECT_INIT (0, 0, 200, 100)));

  g_assert_true (gsk_path_in_fill (path, &GRAPHENE_POINT_INIT (50, 50), GSK_FILL_RULE_WINDING));
  g_assert_false (gsk_path_in_fill (path, &GRAPHENE_POINT_INIT (200, 200), GSK_FILL_RULE_WINDING));

  g_assert_true (gsk_path_get_closest_point (path, &GRAPHENE_POINT_INIT (200, 200), INFINITY, &point));

  gsk_path_unref (path);
}

/* test quad <> cubic conversions */
static gboolean
collect_path (GskPathOperation        op,
              const graphene_point_t *pts,
              gsize                   n_pts,
              gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  if (op == GSK_PATH_MOVE)
    return TRUE;

  switch (op)
    {
    case GSK_PATH_MOVE:
      break;

    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;

    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder, pts[1].x, pts[1].y,
                                         pts[2].x, pts[2].y);
      break;

    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder, pts[1].x, pts[1].y,
                                          pts[2].x, pts[2].y,
                                          pts[3].x, pts[3].y);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
test_foreach (void)
{
  const char *s = "M 0 0 Q 9 0, 9 9 Q 99 9, 99 18 Z";
  const char *sp = "M 0 0 C 6 0, 9 3, 9 9 C 69 9, 99 12, 99 18 Z";
  GskPath *path, *path2;
  char *s2;
  GskPathBuilder *builder;

  path = gsk_path_parse (s);

  builder = gsk_path_builder_new ();
  gsk_path_foreach (path, GSK_PATH_FOREACH_ALLOW_QUAD, collect_path, builder);
  path2 = gsk_path_builder_free_to_path (builder);
  s2 = gsk_path_to_string (path2);

  g_assert_cmpstr (s, ==, s2);

  gsk_path_unref (path2);
  g_free (s2);

  builder = gsk_path_builder_new ();
  gsk_path_foreach (path, GSK_PATH_FOREACH_ALLOW_QUAD|GSK_PATH_FOREACH_ALLOW_CUBIC, collect_path, builder);
  path2 = gsk_path_builder_free_to_path (builder);
  s2 = gsk_path_to_string (path2);

  g_assert_cmpstr (s, ==, s2);

  gsk_path_unref (path2);
  g_free (s2);

  builder = gsk_path_builder_new ();
  gsk_path_foreach (path, GSK_PATH_FOREACH_ALLOW_CUBIC, collect_path, builder);
  path2 = gsk_path_builder_free_to_path (builder);
  s2 = gsk_path_to_string (path2);

  g_assert_cmpstr (sp, ==, s2);

  gsk_path_unref (path2);
  g_free (s2);
}

/* Test the basics of the path point api */
typedef struct _GskRealPathPoint GskRealPathPoint;
struct _GskRealPathPoint
{
  gsize contour;
  gsize idx;
  float t;
};

static void
test_path_point (void)
{
  GskPath *path;
  GskPathPoint point;
  GskRealPathPoint *rp = (GskRealPathPoint *)&point;
  gboolean ret;
  graphene_point_t pos, center;
  graphene_vec2_t t1, t2, mx;
  float curvature;

  path = gsk_path_parse ("M 0 0 L 100 0 L 100 100 L 0 100 Z");

  ret = gsk_path_get_start_point (path, &point);
  g_assert_true (ret);

  g_assert_true (rp->contour == 0);
  g_assert_true (rp->idx == 1);
  g_assert_true (rp->t == 0);

  ret = gsk_path_get_end_point (path, &point);
  g_assert_true (ret);

  g_assert_true (rp->contour == 0);
  g_assert_true (rp->idx == 4);
  g_assert_true (rp->t == 1);

  ret = gsk_path_get_closest_point (path, &GRAPHENE_POINT_INIT (200, 200), INFINITY, &point);
  g_assert_true (ret);

  g_assert_true (rp->contour == 0);
  g_assert_true (rp->idx == 2);
  g_assert_true (rp->t == 1);

  gsk_path_point_get_position (&point, path, &pos);
  gsk_path_point_get_tangent (&point, path, GSK_PATH_FROM_START, &t1);
  gsk_path_point_get_tangent (&point, path, GSK_PATH_TO_END, &t2);
  curvature = gsk_path_point_get_curvature (&point, path, &center);

  g_assert_true (graphene_point_equal (&pos, &GRAPHENE_POINT_INIT (100, 100)));
  g_assert_true (graphene_vec2_equal (&t1, graphene_vec2_y_axis ()));
  graphene_vec2_negate (graphene_vec2_x_axis (), &mx);
  g_assert_true (graphene_vec2_equal (&t2, &mx));
  g_assert_true (curvature == 0);

  ret = gsk_path_get_closest_point (path, &GRAPHENE_POINT_INIT (100, 50), INFINITY, &point);
  g_assert_true (ret);

  g_assert_true (rp->contour == 0);
  g_assert_true (rp->idx == 2);
  g_assert_true (rp->t == 0.5);

  gsk_path_unref (path);
}

/* Test that gsk_path_builder_add_segment yields the expected results */
static void
test_path_segments (void)
{
  struct {
    const char *path;
    graphene_point_t p1;
    graphene_point_t p2;
    const char *result;
  } tests[] = {
    {
      "M 0 0 L 100 0 L 50 50 Z",
      { 100, 0 },
      { 50, 50 },
      "M 100 0 L 50 50"
    },
    {
      "M 0 0 L 100 0 L 50 50 Z",
      { 50, 0 },
      { 70, 0 },
      "M 50 0 L 70 0"
    },
    {
      "M 0 0 L 100 0 L 50 50 Z",
      { 70, 0 },
      { 50, 0 },
      "M 70 0 L 100 0 L 50 50 L 0 0 L 50 0"
    },
    {
      "M 0 0 L 100 0 L 50 50 Z",
      { 50, 0 },
      { 50, 50 },
      "M 50 0 L 100 0 L 50 50"
    },
    {
      "M 0 0 L 100 0 L 50 50 Z",
      { 100, 0 },
      { 100, 0 },
      "M 100 0 L 50 50 L 0 0 L 100 0"
    }
  };

  for (unsigned int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      GskPath *path;
      GskPathPoint p1, p2;
      GskPathBuilder *builder;
      GskPath *result;
      char *str;

      path = gsk_path_parse (tests[i].path);
      g_assert_true (gsk_path_get_closest_point (path, &tests[i].p1, INFINITY, &p1));
      g_assert_true (gsk_path_get_closest_point (path, &tests[i].p2, INFINITY, &p2));

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, path, &p1, &p2);
      result = gsk_path_builder_free_to_path (builder);
      str = gsk_path_to_string (result);

      g_assert_cmpstr (str, ==, tests[i].result);

      g_free (str);
      gsk_path_unref (path);
    }
}

static void
test_bad_in_fill (void)
{
  GskPath *path;
  gboolean inside;

  /* A fat Cantarell W */
  path = gsk_path_parse ("M -2 694 M 206.1748046875 704 L 390.9371337890625 704 L 551.1888427734375 99.5035400390625 L 473.0489501953125 99.5035400390625 L 649.1048583984375 704 L 828.965087890625 704 L 1028.3077392578125 10 L 857.8111572265625 10 L 710.0489501953125 621.251708984375 L 775.9720458984375 598.426513671875 L 614.5245361328125 14.0489501953125 L 430.2237548828125 14.0489501953125 L 278.6783447265625 602.230712890625 L 330.0909423828125 602.230712890625 L 195.88818359375 10 L 5.7342529296875 10 L 206.1748046875 704 Z");

  /* The midpoint of the right foot of a fat Cantarell X */
  inside = gsk_path_in_fill (path, &GRAPHENE_POINT_INIT (552.360107, 704.000000), GSK_FILL_RULE_WINDING);

  g_assert_false (inside);

  gsk_path_unref (path);
}

/* Test that path_in_fill implicitly closes contours. I think this is wrong,
 * but it is what "everybody" does.
 */
static void
test_unclosed_in_fill (void)
{
  GskPath *path;

  path = gsk_path_parse ("M 0 0 L 0 100 L 100 100 L 100 0 Z");
  g_assert_true (gsk_path_in_fill (path, &GRAPHENE_POINT_INIT (50, 50), GSK_FILL_RULE_WINDING));
  gsk_path_unref (path);

  path = gsk_path_parse ("M 0 0 L 0 100 L 100 100 L 100 0");
  g_assert_true (gsk_path_in_fill (path, &GRAPHENE_POINT_INIT (50, 50), GSK_FILL_RULE_WINDING));
  gsk_path_unref (path);
}

/* Test that all the gsk_path_builder_add methods close the current
 * contour in the end and do not change the current point.
 */
static void
test_path_builder_add (void)
{
  GskPathBuilder *builder;
  GskPath *path, *path2;
  char *s;
  cairo_path_t *cpath;
  cairo_surface_t *surface;
  cairo_t *cr;
  PangoLayout *layout;
  GskPathPoint point1, point2;

#define N_ADD_METHODS 8

  path = gsk_path_parse ("M 10 10 L 100 100");

  gsk_path_get_closest_point (path, &GRAPHENE_POINT_INIT (50, 50), INFINITY, &point1);
  gsk_path_get_end_point (path, &point2);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 100, 100);
  cr = cairo_create (surface);
  cairo_move_to (cr, 10, 10);
  cairo_line_to (cr, 20, 30);
  cpath = cairo_copy_path (cr);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_text (layout, "ABC", -1);

  for (unsigned int i = 0; i < N_ADD_METHODS; i++)
    {
      builder = gsk_path_builder_new ();
      gsk_path_builder_move_to (builder, 123, 456);

      switch (i)
        {
        case 0:
          gsk_path_builder_add_path (builder, path);
          break;

        case 1:
          gsk_path_builder_add_reverse_path (builder, path);
          break;

        case 2:
          gsk_path_builder_add_segment (builder, path, &point1, &point2);
          break;

        case 3:
          gsk_path_builder_add_cairo_path (builder, cpath);
          break;

        case 4:
          gsk_path_builder_add_layout (builder, layout);
          break;

        case 5:
          gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (0, 0, 10, 10));
          break;

        case 6:
          {
            GskRoundedRect rect;

            gsk_rounded_rect_init (&rect,
                                   &GRAPHENE_RECT_INIT (0, 0, 100, 100),
                                   &GRAPHENE_SIZE_INIT (10, 20),
                                   &GRAPHENE_SIZE_INIT (20, 30),
                                   &GRAPHENE_SIZE_INIT (0, 0),
                                   &GRAPHENE_SIZE_INIT (10, 10));
            gsk_path_builder_add_rounded_rect (builder, &rect);
          }
          break;

        case 7:
          gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (0, 0), 10);
          break;

        default:
          g_assert_not_reached ();
        }

      gsk_path_builder_rel_line_to (builder, 10, 0);
      path2 = gsk_path_builder_free_to_path (builder);

      s = gsk_path_to_string (path2);
      g_assert_true (g_str_has_prefix (s, "M 123 456"));
      g_assert_true (g_str_has_suffix (s, "M 123 456 L 133 456"));
      g_free (s);
      gsk_path_unref (path2);
    }

  g_object_unref (layout);

  cairo_path_destroy (cpath);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  gsk_path_unref (path);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/rsvg-parse", test_rsvg_parse);
  g_test_add_func ("/path/empty", test_empty_path);
  g_test_add_func ("/path/rect", test_rect_path);
  g_test_add_func ("/path/foreach", test_foreach);
  g_test_add_func ("/path/point", test_path_point);
  g_test_add_func ("/path/segments", test_path_segments);
  g_test_add_func ("/path/bad-in-fill", test_bad_in_fill);
  g_test_add_func ("/path/unclosed-in-fill", test_unclosed_in_fill);
  g_test_add_func ("/path/builder/add", test_path_builder_add);

  return g_test_run ();
}

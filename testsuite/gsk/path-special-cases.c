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
    { "M10 20 Q30 40 50 60", "M 10 20 C 23.3333333 33.3333333, 36.6666667 46.6666667, 50 60" },
    { "M10 20 Q30 40 50 60,70,80-90 100",
      "M 10 20 C 23.3333333 33.3333333, 36.6666667 46.6666667, 50 60 C 63.3333333 73.3333333, 16.6666667 86.6666667, -90 100" },
    { "m10 20 q 30,40 50 60-70,80 90 100",
      "M 10 20 C 30 46.6666667, 46.6666667 66.6666667, 60 80 C 13.3333333 133.3333333, 43.3333333 166.6666667, 150 180" },
    // smooth quadratic curveto
    { "M10 20 T30 40", "M 10 20 C 10 20, 16.6666667 26.6666667, 30 40" },
    { "M10 20 Q30 40 50 60 T70 80",
      "M 10 20 C 23.3333333 33.3333333, 36.6666667 46.6666667, 50 60 C 63.3333333 73.3333333, 70 80, 70 80" },
    { "m10 20 q 30,40 50 60t-70,80",
      "M 10 20 C 30 46.6666667, 46.6666667 66.6666667, 60 80 C 73.3333333 93.3333333, 50 120, -10 160" },
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
              /* Preferred, but doesn't work, because
               * gsk_path_print() prints numbers with
               * insane accuracy */
              /* g_assert_cmpstr (tests[i].out, ==, string); */
              path = gsk_path_parse (tests[i].out);
              g_assert_nonnull (path);

              string2 = gsk_path_to_string (path);
              gsk_path_unref (path);

              g_assert_cmpstr (string, ==, string2);
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

/* Test that circles and rectangles serialize as expected and can be
 * round-tripped through strings.
 */
static void
test_serialize_custom_contours (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  GskPath *path1;
  char *string;
  char *string1;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (100, 100), 50);
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (111, 222, 333, 444));
  path = gsk_path_builder_free_to_path (builder);

  string = gsk_path_to_string (path);
  g_assert_cmpstr ("M 150 100 A 50 50 0 0 0 50 100 A 50 50 0 0 0 150 100 z M 111 222 h 333 v 444 h -333 z", ==, string);

  path1 = gsk_path_parse (string);
  string1 = gsk_path_to_string (path1);
  g_assert_cmpstr (string, ==, string1);

  g_free (string);
  g_free (string1);
  gsk_path_unref (path);
  gsk_path_unref (path1);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/rsvg-parse", test_rsvg_parse);
  g_test_add_func ("/path/serialize-custom-contours", test_serialize_custom_contours);

  return g_test_run ();
}

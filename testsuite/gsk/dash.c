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

#include "gsk/gskpathdashprivate.h"

static gboolean
build_path (GskPathOperation        op,
            const graphene_point_t *pts,
            gsize                   n_pts,
            float                   weight,
            gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  switch (op)
  {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;

    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_CURVE:
      gsk_path_builder_curve_to (builder, pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
      break;

    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builder, pts[1].x, pts[1].y, pts[2].x, pts[2].y, weight);
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

static void
test_simple (void)
{
  const struct {
    const char *test;
    float dash[4];
    gsize n_dash;
    float dash_offset;
    const char *result;
  } tests[] = {
    /* a line with a dash of a quarter its size, very simple test */
    {
      "M 0 0 L 20 0",
      { 5, }, 1, 0.f,
      "M 0 0 L 5 0 M 10 0 L 15 0",
    },
    /* a square with a dash of half its size, another simple test */
    {
      "M 0 0 h 10 v 10 h -10 z",
      { 5, }, 1, 0.f,
      "M 10 0 L 10 5 M 10 10 L 5 10 M 0 10 L 0 5 M 0 0 L 5 0"
    },
    /* a square smaller than the dash, make sure it closes */
    {
      "M 0 0 h 10 v 10 h -10 z",
      { 50, }, 1, 0.f,
      "M 0 0 L 10 0 L 10 10 L 0 10 Z"
    },
    /* a square exactly the dash's size, make sure it still closes */
    {
      "M 0 0 h 10 v 10 h -10 z",
      { 40, }, 1, 0.f,
      "M 0 0 L 10 0 L 10 10 L 0 10 Z"
    },
    /* a dash with offset */
    {
      "M 0 0 h 10 v 10 h -10 z",
      { 5, }, 1, 2.5f,
      "M 7.5 0 L 10 0 L 10 2.5 M 10 7.5 L 10 10 L 7.5 10 M 2.5 10 L 0 10 L 0 7.5 M 0 2.5 L 0 0 L 2.5 0"
    },
    /* a dash with offset, but this time the rect isn't closed */
    {
      "M 0 0 L 10 0 L 10 10 L 0 10 L 0 0",
      { 5, }, 1, 2.5f,
      "M 0 0 L 2.5 0 M 7.5 0 L 10 0 L 10 2.5 M 10 7.5 L 10 10 L 7.5 10 M 2.5 10 L 0 10 L 0 7.5 M 0 2.5 L 0 0"
    },
    /* a dash with offset into an empty dash */
    {
      "M 0 0 h 10 v 10 h -10 z",
      { 5, }, 1, 7.5f,
      "M 2.5 0 L 7.5 0 M 10 2.5 L 10 7.5 M 7.5 10 L 2.5 10 M 0 7.5 L 0 2.5"
    },
    /* a dash with offset where the whole rectangle fits into one element - make sure it closes */
    {
      "M 0 0 h 10 v 10 h -10 z",
      { 1, 1, 100 }, 3, 3.f,
      "M 0 0 L 10 0 L 10 10 L 0 10 Z"
    },
    /* a dash with 0-length elements, aka dotting */
    {
      "M 0 0 h 10 v 10 h -10 z",
      { 0, 5 }, 2, 0.f,
      "M 5 0 M 10 0 M 10 5 M 10 10 M 5 10 M 0 10 M 0 5 M 0 0"
    },
    /* a dash of a circle */
    {
      "M 10 5 O 10 10, 5 10, 0.70710676908493042 O 0 10, 0 5, 0.70710676908493042 O 0 0, 5 0, 0.70710676908493042 O 10 0, 10 5, 0.70710676908493042 Z",
      { 32, }, 1, 0.f,
      "M 10 5 O 10 10, 5 10, 0.70710676908493042 O 0 10, 0 5, 0.70710676908493042 O 0 0, 5 0, 0.70710676908493042 O 10 0, 10 5, 0.70710676908493042 Z",
    },
    /* a dash with offset and 2 contours */
    {
      "M 10 10 h 10 v 10 h -10 z M 20 20 h 10 v 10 h -10 z",
      { 5, }, 1, 2.5f,
      "M 17.5 10 L 20 10 L 20 12.5 M 20 17.5 L 20 20 L 17.5 20 M 12.5 20 L 10 20 L 10 17.5 M 10 12.5 L 10 10 L 12.5 10 "
      "M 27.5 20 L 30 20 L 30 22.5 M 30 27.5 L 30 30 L 27.5 30 M 22.5 30 L 20 30 L 20 27.5 M 20 22.5 L 20 20 L 22.5 20"
    },
  };
  GskPath *path, *result;
  GskPathBuilder *builder;
  GskStroke *stroke;
  char *s;

  for (gsize i = 0; i < G_N_ELEMENTS(tests); i++)
    {
      stroke = gsk_stroke_new (1);
      gsk_stroke_set_dash (stroke, tests[i].dash, tests[i].n_dash);
      gsk_stroke_set_dash_offset (stroke, tests[i].dash_offset);

      path = gsk_path_parse (tests[i].test);
      g_assert_nonnull (path);
      s = gsk_path_to_string (path);
      g_assert_cmpstr (s, ==, tests[i].test);
      g_free (s);

      builder = gsk_path_builder_new ();
      gsk_path_dash (path, stroke, 0.5, build_path, builder);
      result = gsk_path_builder_free_to_path (builder);

      s = gsk_path_to_string (result);
      g_assert_cmpstr (s, ==, tests[i].result);
      g_free (s);

      gsk_path_unref (result);
      gsk_stroke_free (stroke);
      gsk_path_unref (path);
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dash/simple", test_simple);

  return g_test_run ();
}

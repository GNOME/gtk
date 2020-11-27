/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include <gtk/gtk.h>

/* Test that single-point contours don't crash the stroker */
static void
test_point_to_stroke (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  GskStroke *stroke;
  GskPath *path1;
  char *string;

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 100, 100);
  gsk_path_builder_curve_to (builder, 190, 110,
                                      200, 120,
                                      210, 210);
  gsk_path_builder_curve_to (builder, 220, 210,
                                      230, 200,
                                      230, 100);
  gsk_path_builder_move_to (builder, 200, 200);

  path = gsk_path_builder_free_to_path (builder);

  string = gsk_path_to_string (path);
  g_assert_cmpstr (string, ==, "M 100 100 C 190 110, 200 120, 210 210 C 220 210, 230 200, 230 100 M 200 200");
  g_free (string);

  stroke = gsk_stroke_new (20.f);
  path1 = gsk_path_stroke (path, stroke);
  gsk_stroke_free (stroke);

  g_assert_nonnull (path1);
  gsk_path_unref (path1);

  gsk_path_unref (path);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/point_to_stroke", test_point_to_stroke);

  return g_test_run ();
}

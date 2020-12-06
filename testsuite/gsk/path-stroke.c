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

/* Test that the offset curves are generally where they need to be */

static void
check_stroke_at_position (GskPathMeasure *measure,
                          GskStroke      *stroke,
                          GskPathMeasure *stroke_measure,
                          float           position)
{
  graphene_point_t p;
  graphene_point_t s;
  float w;
  float tolerance;
  float d;

  w = gsk_stroke_get_line_width (stroke);
  tolerance = gsk_path_measure_get_tolerance (stroke_measure);

  gsk_path_measure_get_point (measure, position, &p, NULL);
  gsk_path_measure_get_closest_point (stroke_measure, &p, &s);

  d = graphene_point_distance (&p, &s, NULL, NULL);
  g_assert_cmpfloat (d, <=, w/2 + tolerance);
}

static void
check_stroke_distance (GskPath        *path,
                       GskPathMeasure *measure,
                       GskStroke      *stroke,
                       GskPath        *stroke_path)
{
  GskPathMeasure *stroke_measure;
  float length;
  float t;
  int i;

  stroke_measure = gsk_path_measure_new_with_tolerance (stroke_path, 0.1);
  length = gsk_path_measure_get_length (measure);

  for (i = 0; i < 1000; i++)
    {
      t = g_test_rand_double_range (0, length);
      check_stroke_at_position (measure, stroke, stroke_measure, t);
    }

  gsk_path_measure_unref (stroke_measure);
}

static void
test_rect_stroke_distance (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  GskPathMeasure *measure;
  GskPath *stroke_path;
  GskStroke *stroke;

  builder = gsk_path_builder_new ();

  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (0, 0, 100, 100));

  path = gsk_path_builder_free_to_path (builder);

  stroke = gsk_stroke_new (10);

  measure = gsk_path_measure_new (path);
  stroke_path = gsk_path_stroke (path, stroke);

  check_stroke_distance (path, measure, stroke, stroke_path);

  gsk_stroke_free (stroke);

  gsk_path_unref (stroke_path);
  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
}

static void
test_circle_stroke_distance (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  GskPathMeasure *measure;
  GskPath *stroke_path;
  GskStroke *stroke;

  builder = gsk_path_builder_new ();

  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (100, 100), 50);

  path = gsk_path_builder_free_to_path (builder);

  stroke = gsk_stroke_new (10);

  measure = gsk_path_measure_new (path);
  stroke_path = gsk_path_stroke (path, stroke);

  check_stroke_distance (path, measure, stroke, stroke_path);

  gsk_stroke_free (stroke);

  gsk_path_unref (stroke_path);
  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
}

static void
test_path_stroke_distance (void)
{
  GskPath *path;
  GskPathMeasure *measure;
  GskPath *stroke_path;
  GskStroke *stroke;

  path = gsk_path_parse ("M 250 150 A 100 100 0 0 0 50 150 A 100 100 0 0 0 250 150 z M 100 100 h 100 v 100 h -100 z M 300 150 C 300 50, 400 50, 400 150 C 400 250, 500 250, 500 150 L 600 150 L 530 190");

  stroke = gsk_stroke_new (10);

  measure = gsk_path_measure_new (path);
  stroke_path = gsk_path_stroke (path, stroke);

  check_stroke_distance (path, measure, stroke, stroke_path);

  gsk_stroke_free (stroke);

  gsk_path_unref (stroke_path);
  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/stroke/point", test_point_to_stroke);
  g_test_add_func ("/stroke/rect/distance", test_rect_stroke_distance);
  g_test_add_func ("/stroke/circle/distance", test_circle_stroke_distance);
  g_test_add_func ("/stroke/path/distance", test_path_stroke_distance);

  return g_test_run ();
}

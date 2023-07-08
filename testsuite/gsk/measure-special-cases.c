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


static void
test_bad_split (void)
{
  GskPath *path, *path1;
  GskPathMeasure *measure, *measure1;
  GskPathBuilder *builder;
  float split, length, epsilon;

  /* An example that was isolated from the /path/segment test path.c
   * It shows how uneven parametrization of cubics can lead to bad
   * lengths reported by the measure.
   */

  path = gsk_path_parse ("M 0 0 C 2 0 4 0 4 0");

  measure = gsk_path_measure_new (path);
  split = 2.962588;
  length = gsk_path_measure_get_length (measure);
  epsilon = MAX (length / 256, 1.f / 1024);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_segment (builder, measure, 0, split);
  path1 = gsk_path_builder_free_to_path (builder);
  measure1 = gsk_path_measure_new (path1);

  g_assert_cmpfloat_with_epsilon (split, gsk_path_measure_get_length (measure1), epsilon);

  gsk_path_measure_unref (measure1);
  gsk_path_unref (path1);
  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
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

static void
test_rect (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  GskPathMeasure *measure;
  GskPathPoint point;
  graphene_point_t p;
  graphene_vec2_t tangent, expected_tangent;
  float length;
  gboolean ret;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (0, 0, 100, 50));
  path = gsk_path_builder_free_to_path (builder);
  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);

  g_assert_true (length == 300);

#define TEST_POS_AT(distance, X, Y) \
  ret = gsk_path_measure_get_point (measure, distance, &point); \
  g_assert_true (ret); \
  gsk_path_point_get_position (path, &point, &p); \
  g_assert_true (graphene_point_near (&p, &GRAPHENE_POINT_INIT (X, Y), 0.01)); \
  ret = gsk_path_get_closest_point (path, &GRAPHENE_POINT_INIT (X, Y), INFINITY, &point); \
  g_assert_true (ret); \
  if (distance < length) \
    g_assert_true (fabs (gsk_path_measure_get_distance (measure, &point) - distance) < 0.01); \
  else \
    g_assert_true (fabs (gsk_path_measure_get_distance (measure, &point)) < 0.01); \
  gsk_path_point_get_position (path, &point, &p); \
  g_assert_true (graphene_point_near (&p, &GRAPHENE_POINT_INIT (X, Y), 0.01)); \

#define TEST_TANGENT_AT(distance, x1, y1, x2, y2) \
  ret = gsk_path_measure_get_point (measure, distance, &point); \
  g_assert_true (ret); \
  gsk_path_point_get_tangent (path, &point, GSK_PATH_START, &tangent); \
  g_assert_true (graphene_vec2_near (&tangent, graphene_vec2_init (&expected_tangent, x1, y1), 0.01)); \
  gsk_path_point_get_tangent (path, &point, GSK_PATH_END, &tangent); \
  g_assert_true (graphene_vec2_near (&tangent, graphene_vec2_init (&expected_tangent, x2, y2), 0.01)); \

#define TEST_POS_AT2(distance, X, Y, expected_distance) \
  ret = gsk_path_measure_get_point (measure, distance, &point); \
  g_assert_true (ret); \
  gsk_path_point_get_position (path, &point, &p); \
  g_assert_true (graphene_point_near (&p, &GRAPHENE_POINT_INIT (X, Y), 0.01)); \
  ret = gsk_path_get_closest_point (path, &GRAPHENE_POINT_INIT (X, Y), INFINITY, &point); \
  g_assert_true (ret); \
  g_assert_true (fabs (gsk_path_measure_get_distance (measure, &point) - expected_distance) < 0.01); \
  gsk_path_point_get_position (path, &point, &p); \
  g_assert_true (graphene_point_near (&p, &GRAPHENE_POINT_INIT (X, Y), 0.01)); \

  TEST_POS_AT (0, 0, 0)
  TEST_POS_AT (25, 25, 0)
  TEST_POS_AT (100, 100, 0)
  TEST_POS_AT (110, 100, 10)
  TEST_POS_AT (150, 100, 50)
  TEST_POS_AT (175, 75, 50)
  TEST_POS_AT (250, 0, 50)
  TEST_POS_AT (260, 0, 40)
  TEST_POS_AT2 (300, 0, 0, 0)

  TEST_TANGENT_AT (0, 0, -1, 1, 0)
  TEST_TANGENT_AT (50, 1, 0, 1, 0)
  TEST_TANGENT_AT (100, 1, 0, 0, 1)
  TEST_TANGENT_AT (125, 0, 1, 0, 1)
  TEST_TANGENT_AT (150, 0, 1, -1, 0)
  TEST_TANGENT_AT (200, -1, 0, -1, 0)
  TEST_TANGENT_AT (250, -1, 0, 0, -1)
  TEST_TANGENT_AT (275, 0, -1, 0, -1)

  gsk_path_measure_unref (measure);
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (100, 50, -100, -50));
  path = gsk_path_builder_free_to_path (builder);
  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);

  g_assert_true (length == 300);

  TEST_POS_AT (0, 100, 50)
  TEST_POS_AT (25, 75, 50)
  TEST_POS_AT (100, 0, 50)
  TEST_POS_AT (110, 0, 40)
  TEST_POS_AT (150, 0, 0)
  TEST_POS_AT (175, 25, 0)
  TEST_POS_AT (250, 100, 0)
  TEST_POS_AT (260, 100, 10)
  TEST_POS_AT (300, 100, 50)

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (100, 0, -100, 50));
  path = gsk_path_builder_free_to_path (builder);
  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);

  g_assert_true (length == 300);

  TEST_POS_AT (0, 100, 0)
  TEST_POS_AT (25, 75, 0)
  TEST_POS_AT (100, 0, 0)
  TEST_POS_AT (110, 0, 10)
  TEST_POS_AT (150, 0, 50)
  TEST_POS_AT (175, 25, 50)
  TEST_POS_AT (250, 100, 50)
  TEST_POS_AT (260, 100, 40)
  TEST_POS_AT (300, 100, 0)

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (0, 0, 100, 0));
  path = gsk_path_builder_free_to_path (builder);
  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);

  g_assert_true (length == 200);

  TEST_POS_AT2 (0, 0, 0, 0)
  TEST_POS_AT2 (25, 25, 0, 25)
  TEST_POS_AT2 (100, 100, 0, 100)
  TEST_POS_AT2 (110, 90, 0, 90)
  TEST_POS_AT2 (200, 0, 0, 0)

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (100, 0, -100, 0));
  path = gsk_path_builder_free_to_path (builder);
  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);

  g_assert_true (length == 200);

  /* These cases are ambiguous */
  TEST_POS_AT2 (0, 100, 0, 0)
  TEST_POS_AT2 (25, 75, 0, 25)
  TEST_POS_AT2 (100, 0, 0, 100)
  TEST_POS_AT2 (110, 10, 0, 110)
  TEST_POS_AT2 (200, 100, 0, 0)

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (0, 100, 0, -100));
  path = gsk_path_builder_free_to_path (builder);
  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);

  g_assert_true (length == 200);

  /* These cases are ambiguous */
  TEST_POS_AT2 (0, 0, 100, 0)
  TEST_POS_AT2 (25, 0, 75, 25)
  TEST_POS_AT2 (100, 0, 0, 100)
  TEST_POS_AT2 (110, 0, 10, 110)
  TEST_POS_AT2 (200, 0, 100, 0)

#undef TEST_POS_AT
#undef TEST_POS_AT2
#undef TEST_TANGENT_AT

  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/measure/bad-split", test_bad_split);
  g_test_add_func ("/measure/bad-in-fill", test_bad_in_fill);
  g_test_add_func ("/measure/unclosed-in-fill", test_unclosed_in_fill);
  g_test_add_func ("/measure/rect", test_rect);

  return g_test_run ();
}

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

#include "path-utils.h"
#include "gsk/gskpathprivate.h"
#include "gsk/gskboundingboxprivate.h"

#define N_PATHS 3
static void
test_in_fill_union (void)
{
  GskPath *path, *paths[N_PATHS];
  GskPathBuilder *builder;
  guint i, j, k;

  for (i = 0; i < 100; i++)
    {
      builder = gsk_path_builder_new ();

      for (k = 0; k < N_PATHS; k++)
        {
          paths[k] = create_random_path (G_MAXUINT);
          gsk_path_builder_add_path (builder, paths[k]);
        }

      path = gsk_path_builder_free_to_path (builder);

      for (j = 0; j < 100; j++)
        {
          graphene_point_t test = GRAPHENE_POINT_INIT (g_test_rand_double_range (-1000, 1000),
                                                       g_test_rand_double_range (-1000, 1000));
          GskFillRule fill_rule;

          for (fill_rule = GSK_FILL_RULE_WINDING; fill_rule <= GSK_FILL_RULE_EVEN_ODD; fill_rule++)
            {
              guint n_in_fill = 0;
              gboolean in_fill;

              for (k = 0; k < N_PATHS; k++)
                {
                  if (gsk_path_in_fill (paths[k], &test, GSK_FILL_RULE_EVEN_ODD))
                    n_in_fill++;
                }

              in_fill = gsk_path_in_fill (path, &test, GSK_FILL_RULE_EVEN_ODD);
              switch (fill_rule)
                {
                case GSK_FILL_RULE_WINDING:
                  if (n_in_fill == 0)
                    g_assert_false (in_fill);
                  else if (n_in_fill == 1)
                    g_assert_true (in_fill);
                  /* else we can't say anything because the winding rule doesn't give enough info */
                  break;

                case GSK_FILL_RULE_EVEN_ODD:
                  g_assert_cmpint (in_fill, ==, n_in_fill & 1);
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }
            }
        }

      gsk_path_unref (path);

      for (k = 0; k < N_PATHS; k++)
        gsk_path_unref (paths[k]);
    }
}
#undef N_PATHS

/* This is somewhat sucky because using foreach breaks up the contours
 * (like rects and circles) and replaces everything with the standard
 * contour.
 * But at least it extensively tests the standard contour.
 */
static gboolean
rotate_path_cb (GskPathOperation        op,
                const graphene_point_t *pts,
                gsize                   n_pts,
                float                   weight,
                gpointer                user_data)
{
  GskPathBuilder **builders = user_data;

  switch (op)
  {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builders[0], pts[0].x, pts[0].y);
      gsk_path_builder_move_to (builders[1], pts[0].y, -pts[0].x);
      break;

    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builders[0]);
      gsk_path_builder_close (builders[1]);
      break;

    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builders[0], pts[1].x, pts[1].y);
      gsk_path_builder_line_to (builders[1], pts[1].y, -pts[1].x);
      break;

    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builders[0], pts[1].x, pts[1].y, pts[2].x, pts[2].y);
      gsk_path_builder_quad_to (builders[1], pts[1].y, -pts[1].x, pts[2].y, -pts[2].x);
      break;

    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builders[0], pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
      gsk_path_builder_cubic_to (builders[1], pts[1].y, -pts[1].x, pts[2].y, -pts[2].x, pts[3].y, -pts[3].x);
      break;

    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builders[0], pts[1].x, pts[1].y, pts[2].x, pts[2].y, weight);
      gsk_path_builder_conic_to (builders[1], pts[1].y, -pts[1].x, pts[2].y, -pts[2].x, weight);
      break;

    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

static void
test_in_fill_rotated (void)
{
  GskPath *path;
  GskPathBuilder *builders[2];
  GskPath *paths[2];
  guint i, j;

#define N_FILL_RULES 2
  /* if this triggers, you added a new enum value to GskFillRule, so the define above needs
   * an update */
  g_assert_null (g_enum_get_value (g_type_class_ref (GSK_TYPE_FILL_RULE), N_FILL_RULES));

  for (i = 0; i < 100; i++)
    {
      path = create_random_path (G_MAXUINT);
      builders[0] = gsk_path_builder_new ();
      builders[1] = gsk_path_builder_new ();
      /* Use -1 here because we want all the flags, even future additions */
      gsk_path_foreach (path, -1, rotate_path_cb, builders);
      gsk_path_unref (path);

      paths[0] = gsk_path_builder_free_to_path (builders[0]);
      paths[1] = gsk_path_builder_free_to_path (builders[1]);

      for (j = 0; j < 100; j++)
        {
          GskFillRule fill_rule = g_test_rand_int_range (0, N_FILL_RULES);
          float x = g_test_rand_double_range (-1000, 1000);
          float y = g_test_rand_double_range (-1000, 1000);

          g_assert_cmpint (gsk_path_in_fill (paths[0], &GRAPHENE_POINT_INIT (x, y), fill_rule),
                           ==,
                           gsk_path_in_fill (paths[1], &GRAPHENE_POINT_INIT (y, -x), fill_rule));
          g_assert_cmpint (gsk_path_in_fill (paths[0], &GRAPHENE_POINT_INIT (y, x), fill_rule),
                           ==,
                           gsk_path_in_fill (paths[1], &GRAPHENE_POINT_INIT (x, -y), fill_rule));
        }

      gsk_path_unref (paths[0]);
      gsk_path_unref (paths[1]);
    }
#undef N_FILL_RULES
}

static void
test_split (void)
{
  GskPath *path, *path1, *path2;
  GskPathMeasure *measure, *measure1, *measure2;
  float length, length1, length2;
  GskPathBuilder *builder;
  float split, epsilon;
  GskPathPoint point0, point1, point2;
  float tolerance = 0.5;

  for (int i = 0; i < 100; i++)
    {
      if (g_test_verbose ())
        g_test_message ("path %u", i);

      path = create_random_path (1);
      measure = gsk_path_measure_new_with_tolerance (path, tolerance);

      length = gsk_path_measure_get_length (measure);
      /* chosen high enough to stop the testsuite from failing */
      epsilon = MAX (length / 250, 1.f / 1024);

      split = g_test_rand_double_range (0, length);

      if (!gsk_path_get_start_point (path, &point0) ||
          !gsk_path_measure_get_point (measure, split, &point1) ||
          !gsk_path_get_end_point (path, &point2))
        {
          gsk_path_unref (path);
          gsk_path_measure_unref (measure);
          continue;
        }

      if (gsk_path_point_equal (&point0, &point1) ||
          gsk_path_point_equal (&point1, &point2))
        {
          gsk_path_unref (path);
          gsk_path_measure_unref (measure);
          continue;
        }

      g_assert_true (gsk_path_point_compare (&point0, &point1) < 0);
      g_assert_true (gsk_path_point_compare (&point1, &point2) < 0);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, path, &point0, &point1);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new_with_tolerance (path1, tolerance);
      length1 = gsk_path_measure_get_length (measure1);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, path, &point1, &point2);
      path2 = gsk_path_builder_free_to_path (builder);
      measure2 = gsk_path_measure_new_with_tolerance (path2, tolerance);
      length2 = gsk_path_measure_get_length (measure2);

      g_assert_cmpfloat_with_epsilon (length, length1 + length2, epsilon);

      gsk_path_unref (path2);
      gsk_path_unref (path1);
      gsk_path_unref (path);

      gsk_path_measure_unref (measure2);
      gsk_path_measure_unref (measure1);
      gsk_path_measure_unref (measure);
    }
}

static void
test_roundtrip (void)
{
  GskPath *path;
  GskPathMeasure *measure;
  float length;
  float split, epsilon;
  GskPathPoint point;
  float distance;
  float tolerance = 0.5;

  for (int i = 0; i < 100; i++)
    {
      if (g_test_verbose ())
        g_test_message ("path %u", i);

      path = create_random_path (1);
      measure = gsk_path_measure_new_with_tolerance (path, tolerance);

      length = gsk_path_measure_get_length (measure);
      /* chosen high enough to stop the testsuite from failing */
      epsilon = MAX (length / 1000, 1.f / 1024);

      split = g_test_rand_double_range (0, length);

      if (!gsk_path_measure_get_point (measure, split, &point))
        {
          gsk_path_unref (path);
          gsk_path_measure_unref (measure);
          continue;
        }

      distance = gsk_path_point_get_distance (&point, measure);

      g_assert_cmpfloat_with_epsilon (split, distance, epsilon);

      gsk_path_unref (path);
      gsk_path_measure_unref (measure);
    }
}

static void
test_segment (void)
{
  GskPath *path, *path1, *path2, *path3;
  GskPathMeasure *measure, *measure1, *measure2, *measure3;
  GskPathPoint point0, point1, point2, point3;
  float length, length1, length2, length3;
  GskPathBuilder *builder;
  float split1, split2, epsilon;
  float tolerance = 0.5;

  for (int i = 0; i < 100; i++)
    {
      if (g_test_verbose ())
        g_test_message ("path %u", i);

      path = create_random_path (G_MAXUINT);
      measure = gsk_path_measure_new_with_tolerance (path, tolerance);
      length = gsk_path_measure_get_length (measure);

      /* We are accumulating both the split error and the roundtrip error
       * here (on both ends, for the middle segment). So we should expect
       * the epsilon here to be at least 4 times the epsilon we can use
       * in the split and roundtrip tests.
       */
      epsilon = MAX (length / 200, 1.f / 1024);

      split1 = g_test_rand_double_range (0, length);
      split2 = g_test_rand_double_range (split1, length);

      if (!gsk_path_get_start_point (path, &point0) ||
          !gsk_path_measure_get_point (measure, split1, &point1) ||
          !gsk_path_measure_get_point (measure, split2, &point2) ||
          !gsk_path_get_end_point (path, &point3))
        {
          gsk_path_unref (path);
          gsk_path_measure_unref (measure);
          continue;
        }

      if (gsk_path_point_equal (&point0, &point1) ||
          gsk_path_point_equal (&point1, &point2) ||
          gsk_path_point_equal (&point2, &point3))
        {
          gsk_path_unref (path);
          gsk_path_measure_unref (measure);
          continue;
        }

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, path, &point0, &point1);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new_with_tolerance (path1, tolerance);
      length1 = gsk_path_measure_get_length (measure1);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, path, &point1, &point2);
      path2 = gsk_path_builder_free_to_path (builder);
      measure2 = gsk_path_measure_new_with_tolerance (path2, tolerance);
      length2 = gsk_path_measure_get_length (measure2);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, path, &point2, &point3);
      path3 = gsk_path_builder_free_to_path (builder);
      measure3 = gsk_path_measure_new_with_tolerance (path3, tolerance);
      length3 = gsk_path_measure_get_length (measure3);

      g_assert_cmpfloat_with_epsilon (split1, length1, epsilon);
      g_assert_cmpfloat_with_epsilon (split2, length1 + length2, epsilon);
      g_assert_cmpfloat_with_epsilon (length, length1 + length2 + length3, epsilon);

      gsk_path_unref (path3);
      gsk_path_unref (path2);
      gsk_path_unref (path1);
      gsk_path_unref (path);

      gsk_path_measure_unref (measure3);
      gsk_path_measure_unref (measure2);
      gsk_path_measure_unref (measure1);
      gsk_path_measure_unref (measure);
    }
}

static void
test_distance (void)
{
  GskPath *path;
  GskPathMeasure *measure;
  GskPathPoint point = { .contour = 0, .idx = 1, .t = 1 };
  float distance;

  path = gsk_path_parse ("M 20 6.66666677e+30 C 20 60, 50 90, 100 60 C 150 30, 180 30, 180 60");
  measure = gsk_path_measure_new (path);

  distance = gsk_path_point_get_distance (&point, measure);

  /* We just need to get here */
  if (g_test_verbose ())
    g_test_message ("distance is %f", distance);

  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
}

static void
test_circle_bounds (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  graphene_rect_t bounds;
  GskStroke *stroke;
  gboolean ret;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (10, 10), 5);
  path = gsk_path_builder_free_to_path (builder);

  ret = gsk_path_get_bounds (path, &bounds);
  g_assert_true (ret);
  g_assert_true (graphene_rect_equal (&bounds, &GRAPHENE_RECT_INIT (5, 5, 10, 10)));

  stroke = gsk_stroke_new (2);
  ret = gsk_path_get_stroke_bounds (path, stroke, &bounds);
  g_assert_true (ret);
  g_assert_true (graphene_rect_equal (&bounds, &GRAPHENE_RECT_INIT (4, 4, 12, 12)));
  gsk_stroke_free (stroke);

  gsk_path_unref (path);
}

static void
test_rect_bounds (void)
{
  GskPathBuilder *builder;
  GskPath *path;
  graphene_rect_t rect = GRAPHENE_RECT_INIT (10, 10, 20, 30);
  float line_width = 2;
  graphene_rect_t bounds;
  GskStroke *stroke;
  gboolean ret;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, &rect);
  path = gsk_path_builder_free_to_path (builder);

  ret = gsk_path_get_bounds (path, &bounds);
  g_assert_true (ret);
  g_assert_true (graphene_rect_equal (&bounds, &rect));

  stroke = gsk_stroke_new (line_width);

  ret = gsk_path_get_stroke_bounds (path, stroke, &bounds);
  g_assert_true (ret);

  graphene_rect_inset (&rect, - line_width / 2, - line_width / 2);
  g_assert_true (graphene_rect_contains_rect (&bounds, &rect));

  gsk_stroke_free (stroke);
  gsk_path_unref (path);
}

static void
get_offset_point (const graphene_point_t *p,
                  float                   angle,
                  float                   r,
                  graphene_point_t       *q)
{
  q->x = p->x + r * cos (angle);
  q->y = p->y + r * sin (angle);
}

static void
test_general_bounds (void)
{
  float line_width = 2;
  GskStroke *stroke;

  stroke = gsk_stroke_new (line_width);

  for (unsigned int i = 0; i < 100; i++)
    {
      GskPath *path;
      GskPathPoint point;
      GskBoundingBox bounds;
      GskBoundingBox stroke_bounds;
      graphene_point_t pos, pos2;

      path = create_random_path (G_MAXUINT);

      if (!gsk_path_get_start_point (path, &point))
        {
          gsk_path_unref (path);
          continue;
        }

      gsk_path_get_bounding_box (path, &bounds);
      gsk_path_get_stroke_bounding_box (path, stroke, &stroke_bounds);

      do {
        gsk_path_point_get_position (&point, path, &pos);
        g_assert_true (gsk_bounding_box_contains_point (&bounds, &pos));

        for (unsigned int j = 0; j < 8; j++)
          {
            float angle = j * 2 * M_PI / 8;

            get_offset_point (&pos, angle, line_width / 2, &pos2);
            g_assert_true (gsk_bounding_box_contains_point (&stroke_bounds, &pos2));
          }
      } while (gsk_path_get_next (path, &point));

      gsk_path_unref (path);
    }

  gsk_stroke_free (stroke);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/fill/union", test_in_fill_union);
  g_test_add_func ("/path/fill/rotated", test_in_fill_rotated);
  g_test_add_func ("/path/measure/split", test_split);
  g_test_add_func ("/path/measure/roundtrip", test_roundtrip);
  g_test_add_func ("/path/measure/segment", test_segment);
  g_test_add_func ("/path/measure/distance", test_distance);
  g_test_add_func ("/path/bounds/circle", test_circle_bounds);
  g_test_add_func ("/path/bounds/rect", test_rect_bounds);
  g_test_add_func ("/path/bounds/general", test_general_bounds);

  return g_test_run ();
}

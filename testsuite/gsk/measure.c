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

static GskPath *
create_random_degenerate_path (guint max_contours)
{
#define N_DEGENERATE_PATHS 14
  GskPathBuilder *builder;
  guint i;

  builder = gsk_path_builder_new ();

  switch (g_test_rand_int_range (0, N_DEGENERATE_PATHS))
  {
    case 0:
      /* empty path */
      break;

    case 1:
      /* a single point */
      gsk_path_builder_move_to (builder, 
                                g_test_rand_double_range (-1000, 1000),
                                g_test_rand_double_range (-1000, 1000));
      break;

    case 2:
      /* N points */
      for (i = 0; i < MIN (10, max_contours); i++)
        {
          gsk_path_builder_move_to (builder, 
                                    g_test_rand_double_range (-1000, 1000),
                                    g_test_rand_double_range (-1000, 1000));
        }
      break;

    case 3:
      /* 1 closed point */
      gsk_path_builder_move_to (builder, 
                                g_test_rand_double_range (-1000, 1000),
                                g_test_rand_double_range (-1000, 1000));
      gsk_path_builder_close (builder);
      break;

    case 4:
      /* the same point closed N times */
      gsk_path_builder_move_to (builder, 
                                g_test_rand_double_range (-1000, 1000),
                                g_test_rand_double_range (-1000, 1000));
      for (i = 0; i < MIN (10, max_contours); i++)
        {
          gsk_path_builder_close (builder);
        }
      break;

    case 5:
      /* a zero-width and zero-height rect */
      gsk_path_builder_add_rect (builder,
                                 &GRAPHENE_RECT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      0, 0));
      break;

    case 6:
      /* a zero-width rect */
      gsk_path_builder_add_rect (builder,
                                 &GRAPHENE_RECT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      0,
                                                      g_test_rand_double_range (-1000, 1000)));
      break;

    case 7:
      /* a zero-height rect */
      gsk_path_builder_add_rect (builder,
                                 &GRAPHENE_RECT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      0));
      break;

    case 8:
      /* a negative-size rect */
      gsk_path_builder_add_rect (builder,
                                 &GRAPHENE_RECT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 0),
                                                      g_test_rand_double_range (-1000, 0)));
      break;

    case 9:
      /* an absolutely random rect */
      gsk_path_builder_add_rect (builder,
                                 &GRAPHENE_RECT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000)));
      break;

    case 10:
      /* an absolutely random rect */
      gsk_path_builder_add_rect (builder,
                                 &GRAPHENE_RECT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000)));
      break;

    case 11:
      /* an absolutely random circle */
      gsk_path_builder_add_circle (builder,
                                   &GRAPHENE_POINT_INIT (g_test_rand_double_range (-1000, 1000),
                                                         g_test_rand_double_range (-1000, 1000)),
                                   g_test_rand_double_range (1, 1000));
      break;

    case 12:
      /* a zero-length line */
      {
        graphene_point_t point = GRAPHENE_POINT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000));
        gsk_path_builder_move_to (builder, point.x, point.y);
        gsk_path_builder_line_to (builder, point.x, point.y);
      }
      break;

    case 13:
      /* a curve with start == end */
      {
        graphene_point_t point = GRAPHENE_POINT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000));
        gsk_path_builder_move_to (builder, point.x, point.y);
        gsk_path_builder_cubic_to (builder,
                                   g_test_rand_double_range (-1000, 1000),
                                   g_test_rand_double_range (-1000, 1000),
                                   g_test_rand_double_range (-1000, 1000),
                                   g_test_rand_double_range (-1000, 1000),
                                   point.x, point.y);
      }
      break;

    case N_DEGENERATE_PATHS:
    default:
      g_assert_not_reached ();
  }

  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
create_random_path (guint max_contours);

static void
add_shape_contour (GskPathBuilder *builder)
{
#define N_SHAPE_CONTOURS 3
  switch (g_test_rand_int_range (0, N_SHAPE_CONTOURS))
  {
    case 0:
      gsk_path_builder_add_rect (builder,
                                 &GRAPHENE_RECT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (1, 1000),
                                                      g_test_rand_double_range (1, 1000)));
      break;

    case 1:
      gsk_path_builder_add_circle (builder,
                                   &GRAPHENE_POINT_INIT (g_test_rand_double_range (-1000, 1000),
                                                         g_test_rand_double_range (-1000, 1000)),
                                   g_test_rand_double_range (1, 1000));
      break;

    case 2:
      {
        GskPath *path = create_random_path (1);
        gsk_path_builder_add_path (builder, path);
        gsk_path_unref (path);
      }
      break;

    case N_SHAPE_CONTOURS:
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
add_standard_contour (GskPathBuilder *builder)
{
  guint i, n;

  if (g_test_rand_bit ())
    {
      if (g_test_rand_bit ())
        gsk_path_builder_move_to (builder,
                                  g_test_rand_double_range (-1000, 1000),
                                  g_test_rand_double_range (-1000, 1000));
      else
        gsk_path_builder_rel_move_to (builder,
                                      g_test_rand_double_range (-1000, 1000),
                                      g_test_rand_double_range (-1000, 1000));
    }

  /* that 20 is random, but should be enough to get some
   * crazy self-intersecting shapes */
  n = g_test_rand_int_range (1, 20);
  for (i = 0; i < n; i++)
    {
      switch (g_test_rand_int_range (0, 6))
      {
        case 0:
          gsk_path_builder_line_to (builder,
                                    g_test_rand_double_range (-1000, 1000),
                                    g_test_rand_double_range (-1000, 1000));
          break;

        case 1:
          gsk_path_builder_rel_line_to (builder,
                                        g_test_rand_double_range (-1000, 1000),
                                        g_test_rand_double_range (-1000, 1000));
          break;

        case 2:
          gsk_path_builder_quad_to (builder,
                                    g_test_rand_double_range (-1000, 1000),
                                    g_test_rand_double_range (-1000, 1000),
                                    g_test_rand_double_range (-1000, 1000),
                                    g_test_rand_double_range (-1000, 1000));
          break;

        case 3:
          gsk_path_builder_rel_quad_to (builder,
                                        g_test_rand_double_range (-1000, 1000),
                                        g_test_rand_double_range (-1000, 1000),
                                        g_test_rand_double_range (-1000, 1000),
                                        g_test_rand_double_range (-1000, 1000));
          break;

        case 4:
          gsk_path_builder_cubic_to (builder,
                                     g_test_rand_double_range (-1000, 1000),
                                     g_test_rand_double_range (-1000, 1000),
                                     g_test_rand_double_range (-1000, 1000),
                                     g_test_rand_double_range (-1000, 1000),
                                     g_test_rand_double_range (-1000, 1000),
                                     g_test_rand_double_range (-1000, 1000));
          break;

        case 5:
          gsk_path_builder_rel_cubic_to (builder,
                                         g_test_rand_double_range (-1000, 1000),
                                         g_test_rand_double_range (-1000, 1000),
                                         g_test_rand_double_range (-1000, 1000),
                                         g_test_rand_double_range (-1000, 1000),
                                         g_test_rand_double_range (-1000, 1000),
                                         g_test_rand_double_range (-1000, 1000));
          break;

        default:
          g_assert_not_reached();
          break;
      }
    }

  if (g_test_rand_bit ())
    gsk_path_builder_close (builder);
}

static GskPath *
create_random_path (guint max_contours)
{
  GskPathBuilder *builder;
  guint i, n;

  /* 5% chance for a weird shape */
  if (!g_test_rand_int_range (0, 20))
    return create_random_degenerate_path (max_contours);

  builder = gsk_path_builder_new ();
  n = g_test_rand_int_range (1, 10);
  n = MIN (n, max_contours);

  for (i = 0; i < n; i++)
    {
      /* 2/3 of shapes are standard contours */
      if (g_test_rand_int_range (0, 3))
        add_standard_contour (builder);
      else
        add_shape_contour (builder);
    }

  return gsk_path_builder_free_to_path (builder);
}

typedef struct {
  GskPathOperation op;
  graphene_point_t pts[4];
  float weight;
} PathOperation;

static void
test_segment_start (void)
{
  GskPath *path, *path1;
  GskPathMeasure *measure, *measure1;
  GskPathBuilder *builder;
  float epsilon, length;
  guint i;

  path = create_random_path (G_MAXUINT);
  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);
  epsilon = MAX (length / 1024, G_MINFLOAT);

  for (i = 0; i < 100; i++)
    {
      float seg_length = length * i / 100.0f;

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, measure, 0, seg_length);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new (path1);

      if (seg_length == 0)
        g_assert_cmpfloat_with_epsilon (gsk_path_measure_get_length (measure), gsk_path_measure_get_length (measure1), epsilon);
      else
        g_assert_cmpfloat_with_epsilon (seg_length, gsk_path_measure_get_length (measure1), epsilon);

      gsk_path_measure_unref (measure1);
      gsk_path_unref (path1);
    }

  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
}

static void
test_segment_end (void)
{
  GskPath *path, *path1;
  GskPathMeasure *measure, *measure1;
  GskPathBuilder *builder;
  float epsilon, length;
  guint i;

  path = create_random_path (G_MAXUINT);
  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);
  epsilon = MAX (length / 1024, G_MINFLOAT);

  for (i = 0; i < 100; i++)
    {
      float seg_length = length * i / 100.0f;

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, measure, length - seg_length, length);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new (path1);

      if (seg_length == 0)
        g_assert_cmpfloat_with_epsilon (gsk_path_measure_get_length (measure), gsk_path_measure_get_length (measure1), epsilon);
      else
        g_assert_cmpfloat_with_epsilon (seg_length, gsk_path_measure_get_length (measure1), epsilon);

      gsk_path_measure_unref (measure1);
      gsk_path_unref (path1);
    }

  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
}

static void
test_segment_chunk (void)
{
  GskPath *path, *path1, *path2;
  GskPathMeasure *measure, *measure1, *measure2;
  GskPathBuilder *builder;
  float epsilon, length;
  guint i;

  path = create_random_path (G_MAXUINT);
  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);
  epsilon = MAX (length / 1024, G_MINFLOAT);

  for (i = 0; i <= 100; i++)
    {
      float seg_start = length * i / 200.0f;

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, measure, seg_start, seg_start + length / 2);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new (path1);

      g_assert_cmpfloat_with_epsilon (length / 2, gsk_path_measure_get_length (measure1), epsilon);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, measure, seg_start + length / 2, seg_start);
      path2 = gsk_path_builder_free_to_path (builder);
      measure2 = gsk_path_measure_new (path2);

      g_assert_cmpfloat_with_epsilon (length / 2, gsk_path_measure_get_length (measure2), epsilon);

      gsk_path_measure_unref (measure2);
      gsk_path_unref (path2);
      gsk_path_measure_unref (measure1);
      gsk_path_unref (path1);
    }

  gsk_path_measure_unref (measure);
  gsk_path_unref (path);
}

static void
test_segment (void)
{
  GskPath *path, *path1, *path2, *path3;
  GskPathMeasure *measure, *measure1, *measure2, *measure3;
  GskPathBuilder *builder;
  guint i;
  float split1, split2, epsilon, length;

  for (i = 0; i < 1000; i++)
    {
      path = create_random_path (G_MAXUINT);
      measure = gsk_path_measure_new (path);
      length = gsk_path_measure_get_length (measure);
      /* chosen high enough to stop the testsuite from failing */
      epsilon = MAX (length / 64, 1.f / 1024);

      split1 = g_test_rand_double_range (0, length);
      split2 = g_test_rand_double_range (split1, length);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, measure, 0, split1);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new (path1);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, measure, split1, split2);
      path2 = gsk_path_builder_free_to_path (builder);
      measure2 = gsk_path_measure_new (path2);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, measure, split2, length);
      path3 = gsk_path_builder_free_to_path (builder);
      measure3 = gsk_path_measure_new (path3);

      g_assert_cmpfloat_with_epsilon (split1, gsk_path_measure_get_length (measure1), epsilon);
      g_assert_cmpfloat_with_epsilon (split2 - split1, gsk_path_measure_get_length (measure2), epsilon);
      g_assert_cmpfloat_with_epsilon (length - split2, gsk_path_measure_get_length (measure3), epsilon);

      gsk_path_measure_unref (measure2);
      gsk_path_measure_unref (measure1);
      gsk_path_measure_unref (measure);
      gsk_path_unref (path2);
      gsk_path_unref (path1);
      gsk_path_unref (path);
    }
}

static void
test_get_point (void)
{
  static const guint max_contours = 5;
  static const float tolerance = 1.0;
  GskPath *path;
  GskPathMeasure *measure;
  GskPathPoint point;
  guint n_discontinuities;
  float length, offset, last_offset;
  graphene_point_t p, last_point;
  guint i, j;
  gboolean ret;

  for (i = 0; i < 10; i++)
    {
      path = create_random_path (max_contours);
      measure = gsk_path_measure_new_with_tolerance (path, tolerance);
      length = gsk_path_measure_get_length (measure);
      n_discontinuities = 0;

      ret = gsk_path_measure_get_point (measure, 0, &point);
      if (!ret)
        {
          g_assert_true (gsk_path_is_empty (path));
          continue;
        }
      gsk_path_point_get_position (path, &point, &last_point);

      /* FIXME: anything we can test with tangents here? */
      last_offset = 0;

      for (j = 1; j <= 1024; j++)
        {
          offset = length * j / 1024;
          ret = gsk_path_measure_get_point (measure, offset, &point);
          g_assert_true (ret);
          gsk_path_point_get_position (path, &point, &p);

          if (graphene_point_distance (&last_point, &p, NULL, NULL) > 2 * (offset - last_offset))
            {
              n_discontinuities++;
              g_assert_cmpint (n_discontinuities, <, max_contours);
            }

          last_offset = offset;
          last_point = p;
        }

      gsk_path_measure_unref (measure);
      gsk_path_unref (path);
    }
}

static void
test_closest_point (void)
{
  static const float tolerance = 0.5;
  GskPath *path, *path1, *path2;
  GskPathMeasure *measure, *measure1, *measure2;
  GskPathBuilder *builder;
  GskPathPoint point;
  guint i, j;
  gboolean ret;

  if (!g_test_slow ())
    {
      g_test_skip ("Skipping slow test");
      return;
    }

  for (i = 0; i < 10; i++)
    {
      path1 = create_random_path (G_MAXUINT);
      measure1 = gsk_path_measure_new_with_tolerance (path1, tolerance);
      path2 = create_random_path (G_MAXUINT);
      measure2 = gsk_path_measure_new_with_tolerance (path2, tolerance);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_path (builder, path1);
      gsk_path_builder_add_path (builder, path2);
      path = gsk_path_builder_free_to_path (builder);
      measure = gsk_path_measure_new_with_tolerance (path, tolerance);

      for (j = 0; j < 100; j++)
        {
          graphene_point_t test = GRAPHENE_POINT_INIT (g_test_rand_double_range (-1000, 1000),
                                                       g_test_rand_double_range (-1000, 1000));
          graphene_point_t p1, p2, p;
          graphene_vec2_t t1, t2, t;
          float offset1, offset2, offset;
          float distance1, distance2, distance;

          offset1 = offset2 = offset = 0;
          distance1 = distance2 = distance = 0;
          ret = gsk_path_get_closest_point (path1, &test, INFINITY, &point);
          g_assert_true (ret);

          gsk_path_point_get_position (path1, &point, &p1);
          gsk_path_point_get_tangent (path1, &point, GSK_PATH_END, &t1);
          offset1 = gsk_path_measure_get_distance (measure1, &point);
          distance1 = graphene_point_distance (&p1, &test, NULL, NULL);

          ret = gsk_path_get_closest_point (path2, &test, INFINITY, &point);
          g_assert_true (ret);

          gsk_path_point_get_position (path2, &point, &p2);
          gsk_path_point_get_tangent (path2, &point, GSK_PATH_END, &t2);
          offset2 = gsk_path_measure_get_distance (measure2, &point);
          distance2 = graphene_point_distance (&p2, &test, NULL, NULL);

          ret = gsk_path_get_closest_point (path, &test, INFINITY, &point);
          g_assert_true (ret);

          gsk_path_point_get_position (path, &point, &p);
          gsk_path_point_get_tangent (path, &point, GSK_PATH_END, &t);
          offset = gsk_path_measure_get_distance (measure, &point);
          distance = graphene_point_distance (&p, &test, NULL, NULL);

          if (distance1 == distance)
            {
              g_assert_cmpfloat (distance1, ==, distance);
              g_assert_cmpfloat (p1.x, ==, p.x);
              g_assert_cmpfloat (p1.y, ==, p.y);
              g_assert_cmpfloat (offset1, ==, offset);
              g_assert_true (graphene_vec2_equal (&t1, &t));
            }
          else
            {
              g_assert_cmpfloat (distance2, ==, distance);
              g_assert_cmpfloat (p2.x, ==, p.x);
              g_assert_cmpfloat (p2.y, ==, p.y);
              g_assert_cmpfloat_with_epsilon (offset2 + gsk_path_measure_get_length (measure1), offset, MAX (G_MINFLOAT, offset / 1024));
              g_assert_true (graphene_vec2_equal (&t2, &t));
            }
        }

      gsk_path_measure_unref (measure2);
      gsk_path_measure_unref (measure1);
      gsk_path_measure_unref (measure);
      gsk_path_unref (path2);
      gsk_path_unref (path1);
      gsk_path_unref (path);
    }
}

static void
test_closest_point_for_point (void)
{
  static const float tolerance = 0.5;
  GskPath *path;
  GskPathMeasure *measure;
  GskPathPoint point;
  float length, offset, distance;
  graphene_point_t p, closest_point;
  guint i, j;
  gboolean ret;

  if (!g_test_slow ())
    {
      g_test_skip ("Skipping slow test");
      return;
    }

  for (i = 0; i < 100; i++)
    {
      path = create_random_path (G_MAXUINT);
      if (gsk_path_is_empty (path))
        {
          /* empty paths have no closest point to anything */
          gsk_path_unref (path);
          continue;
        }

      measure = gsk_path_measure_new_with_tolerance (path, tolerance);
      length = gsk_path_measure_get_length (measure);

      for (j = 0; j < 100; j++)
        {
          offset = g_test_rand_double_range (0, length);
          ret = gsk_path_measure_get_point (measure, offset, &point);
          g_assert_true (ret);
          gsk_path_point_get_position (path, &point, &p);
          ret = gsk_path_get_closest_point (path, &p, 2 * tolerance, &point);
          g_assert_true (ret);
          gsk_path_point_get_position (path, &point, &closest_point);
          //closest_offset = gsk_path_measure_get_distance (measure, &point);
          distance = graphene_point_distance (&p, &closest_point, NULL, NULL);

          /* should be given due to the TRUE return above, but who knows... */
          g_assert_cmpfloat (distance, <=, 2 * tolerance);
          g_assert_cmpfloat (graphene_point_distance (&p, &closest_point, NULL, NULL), <=, 2 * tolerance);
          /* we can't check offsets here, since we might hit self-intersections
          g_assert_cmpfloat_with_epsilon (closest_offset, offset, 2 * tolerance);
          */
        }

      gsk_path_measure_unref (measure);
      gsk_path_unref (path);
    }
}

#define N_PATHS 3
static void
test_in_fill_union (void)
{
  GskPath *path;
  GskPathMeasure *measure, *measures[N_PATHS];
  GskPathBuilder *builder;
  guint i, j, k;

  for (i = 0; i < 100; i++)
    {
      builder = gsk_path_builder_new ();
      for (k = 0; k < N_PATHS; k++)
        {
          path = create_random_path (G_MAXUINT);
          measures[k] = gsk_path_measure_new (path);
          gsk_path_builder_add_path (builder, path);
          gsk_path_unref (path);
        }
      path = gsk_path_builder_free_to_path (builder);
      measure = gsk_path_measure_new (path);
      gsk_path_unref (path);

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
                  if (gsk_path_in_fill (gsk_path_measure_get_path (measures[k]), &test, GSK_FILL_RULE_EVEN_ODD))
                    n_in_fill++;
                }

              in_fill = gsk_path_in_fill (gsk_path_measure_get_path (measure), &test, GSK_FILL_RULE_EVEN_ODD);

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

      gsk_path_measure_unref (measure);
      for (k = 0; k < N_PATHS; k++)
        {
          gsk_path_measure_unref (measures[k]);
        }
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
  GskPathMeasure *measures[2];
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

      path = gsk_path_builder_free_to_path (builders[0]);
      measures[0] = gsk_path_measure_new (path);
      gsk_path_unref (path);
      path = gsk_path_builder_free_to_path (builders[1]);
      measures[1] = gsk_path_measure_new (path);
      gsk_path_unref (path);

      for (j = 0; j < 100; j++)
        {
          GskFillRule fill_rule = g_random_int_range (0, N_FILL_RULES);
          float x = g_test_rand_double_range (-1000, 1000);
          float y = g_test_rand_double_range (-1000, 1000);
  
          g_assert_cmpint (gsk_path_in_fill (gsk_path_measure_get_path (measures[0]), &GRAPHENE_POINT_INIT (x, y), fill_rule),
                           ==,
                           gsk_path_in_fill (gsk_path_measure_get_path (measures[1]), &GRAPHENE_POINT_INIT (y, -x), fill_rule));
          g_assert_cmpint (gsk_path_in_fill (gsk_path_measure_get_path (measures[0]), &GRAPHENE_POINT_INIT (y, x), fill_rule),
                           ==,
                           gsk_path_in_fill (gsk_path_measure_get_path (measures[1]), &GRAPHENE_POINT_INIT (x, -y), fill_rule));
        }

      gsk_path_measure_unref (measures[0]);
      gsk_path_measure_unref (measures[1]);
    }
#undef N_FILL_RULES
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/measure/segment_start", test_segment_start);
  g_test_add_func ("/measure/segment_end", test_segment_end);
  g_test_add_func ("/measure/segment_chunk", test_segment_chunk);
  g_test_add_func ("/measure/segment", test_segment);
  g_test_add_func ("/measure/get_point", test_get_point);
  g_test_add_func ("/measure/closest_point", test_closest_point);
  g_test_add_func ("/measure/closest_point_for_point", test_closest_point_for_point);
  g_test_add_func ("/measure/in-fill-union", test_in_fill_union);
  g_test_add_func ("/measure/in-fill-rotated", test_in_fill_rotated);

  return g_test_run ();
}

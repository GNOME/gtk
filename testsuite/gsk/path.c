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
#define N_DEGENERATE_PATHS 12
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
    gsk_path_builder_move_to (builder,
                              g_test_rand_double_range (-1000, 1000),
                              g_test_rand_double_range (-1000, 1000));

  /* that 20 is random, but should be enough to get some
   * crazy self-intersecting shapes */
  n = g_test_rand_int_range (1, 20);
  for (i = 0; i < n; i++)
    {
      switch (g_test_rand_int_range (0, 2))
      {
        case 0:
          gsk_path_builder_line_to (builder,
                                    g_test_rand_double_range (-1000, 1000),
                                    g_test_rand_double_range (-1000, 1000));
          break;

        case 1:
          gsk_path_builder_curve_to (builder,
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
  if (g_test_rand_int_range (0, 20))
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

static void
test_create (void)
{
  GskPath *path1, *path2, *built;
  GskPathBuilder *builder;
  guint i;
  char *s;
  GString *str;

  for (i = 0; i < 1000; i++)
    {
      builder = gsk_path_builder_new ();
      path1 = create_random_path (G_MAXUINT);
      gsk_path_builder_add_path (builder, path1);
      path2 = create_random_path (G_MAXUINT);
      gsk_path_builder_add_path (builder, path2);
      built = gsk_path_builder_free_to_path (builder);

      str = g_string_new (NULL);
      gsk_path_print (path1, str);
      if (!gsk_path_is_empty (path1) && !gsk_path_is_empty (path2))
        g_string_append_c (str, ' ');
      gsk_path_print (path2, str);

      s = gsk_path_to_string (built);

      g_assert_cmpstr (s, ==, str->str);
      g_string_free (str, TRUE);
      g_free (s);

      gsk_path_unref (built);
      gsk_path_unref (path2);
      gsk_path_unref (path1);
    }
}

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
      gsk_path_measure_add_segment (measure, builder, 0, seg_length);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new (path1);

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
      gsk_path_measure_add_segment (measure, builder, length - seg_length, length);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new (path1);

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
  GskPath *path, *path1;
  GskPathMeasure *measure, *measure1;
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
      gsk_path_measure_add_segment (measure, builder, seg_start, seg_start + length / 2);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new (path1);

      g_assert_cmpfloat_with_epsilon (length / 2, gsk_path_measure_get_length (measure1), epsilon);

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
      epsilon = MAX (length / 256, 1.f / 1024);

      split1 = g_test_rand_double_range (0, length);
      split2 = g_test_rand_double_range (split1, length);

      builder = gsk_path_builder_new ();
      gsk_path_measure_add_segment (measure, builder, 0, split1);
      path1 = gsk_path_builder_free_to_path (builder);
      measure1 = gsk_path_measure_new (path1);

      builder = gsk_path_builder_new ();
      gsk_path_measure_add_segment (measure, builder, split1, split2);
      path2 = gsk_path_builder_free_to_path (builder);
      measure2 = gsk_path_measure_new (path2);

      builder = gsk_path_builder_new ();
      gsk_path_measure_add_segment (measure, builder, split2, length);
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
  static const float tolerance = 0.5;
  GskPath *path;
  GskPathMeasure *measure;
  guint n_discontinuities;
  float length, offset, last_offset;
  graphene_point_t point, last_point;
  guint i, j;

  for (i = 0; i < 10; i++)
    {
      path = create_random_path (max_contours);
      measure = gsk_path_measure_new_with_tolerance (path, tolerance);
      length = gsk_path_measure_get_length (measure);
      n_discontinuities = 0;

      gsk_path_measure_get_point (measure,
                                  0,
                                  &last_point,
                                  NULL);
      /* FIXME: anything we can test with tangents here? */
      last_offset = 0;

      for (j = 1; j <= 1024; j++)
        {
          offset = length * j / 1024;
          gsk_path_measure_get_point (measure,
                                      offset,
                                      &point,
                                      NULL);

          if (graphene_point_distance (&last_point, &point, NULL, NULL) > offset - last_offset + tolerance)
            {
              n_discontinuities++;
              g_assert_cmpint (n_discontinuities, <, max_contours);
            }

          last_offset = offset;
          last_point = point;
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
  guint i, j;

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
          gboolean found1, found2, found;

          found1 = gsk_path_measure_get_closest_point_full (measure1,
                                                            &test,
                                                            INFINITY,
                                                            &distance1,
                                                            &p1,
                                                            &offset1,
                                                            &t1);
          found2 = gsk_path_measure_get_closest_point_full (measure2,
                                                            &test,
                                                            INFINITY,
                                                            &distance2,
                                                            &p2,
                                                            &offset2,
                                                            &t2);
          found = gsk_path_measure_get_closest_point_full (measure,
                                                           &test,
                                                           INFINITY,
                                                           &distance,
                                                           &p,
                                                           &offset,
                                                           &t);

          if (found1 && (!found2 || distance1 < distance2 + tolerance))
            {
              g_assert_cmpfloat (distance1, ==, distance);
              g_assert_cmpfloat (p1.x, ==, p.x);
              g_assert_cmpfloat (p1.y, ==, p.y);
              g_assert_cmpfloat (offset1, ==, offset);
              g_assert_true (graphene_vec2_equal (&t1, &t));
            }
          else if (found2)
            {
              g_assert_cmpfloat (distance2, ==, distance);
              g_assert_cmpfloat (p2.x, ==, p.x);
              g_assert_cmpfloat (p2.y, ==, p.y);
              g_assert_cmpfloat_with_epsilon (offset2 + gsk_path_measure_get_length (measure1), offset, MAX (G_MINFLOAT, offset / 1024));
              g_assert_true (graphene_vec2_equal (&t2, &t));
            }
          else
            {
              g_assert (!found);
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
  float length, offset, closest_offset, distance;
  graphene_point_t point, closest_point;
  guint i, j;

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

          gsk_path_measure_get_point (measure,
                                      offset,
                                      &point,
                                      NULL);
          g_assert_true (gsk_path_measure_get_closest_point_full (measure,
                                                                  &point,
                                                                  tolerance,
                                                                  &distance,
                                                                  &closest_point,
                                                                  &closest_offset,
                                                                  NULL));
          /* should be given due to the TRUE return above, but who knows... */
          g_assert_cmpfloat (distance, <=, tolerance);
          g_assert_cmpfloat (graphene_point_distance (&point, &closest_point, NULL, NULL), <=, tolerance);
          /* can't do == here because points may overlap if we're unlucky */
          g_assert_cmpfloat (closest_offset, <, offset + tolerance);
        }

      gsk_path_measure_unref (measure);
      gsk_path_unref (path);
    }
}

/* testcases from path_parser.rs in librsvg */
static void
test_from_string (void)
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
    { "M10 20 Q30 40 50 60", "M 10 20 C 23.3333 33.3333, 36.6667 46.6667, 50 60" },
    { "M10 20 Q30 40 50 60,70,80-90 100",
      "M 10 20 C 23.3333 33.3333, 36.6667 46.6667, 50 60 C 63.3333 73.3333, 16.6667 86.6667, -90 100" },
    { "m10 20 q 30,40 50 60-70,80 90 100",
      "M 10 20 C 30 46.6667, 46.6667 66.6667, 60 80 C 13.3333 133.333, 43.3333 166.667, 150 180" },
    // smooth quadratic curveto
    { "M10 20 T30 40", "M 10 20 C 10 20, 16.6667 26.6667, 30 40" },
    { "M10 20 Q30 40 50 60 T70 80",
      "M 10 20 C 23.3333 33.3333, 36.6667 46.6667, 50 60 C 63.3333 73.3333, 70 80, 70 80" },
    { "m10 20 q 30,40 50 60t-70,80",
      "M 10 20 C 30 46.6667, 46.6667 66.6667, 60 80 C 73.3333 93.3333, 50 120, -10 160" },
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
            g_assert_cmpstr (tests[i].out, ==, string);

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

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/create", test_create);
  g_test_add_func ("/path/segment_start", test_segment_start);
  g_test_add_func ("/path/segment_end", test_segment_end);
  g_test_add_func ("/path/segment_chunk", test_segment_chunk);
  g_test_add_func ("/path/segment", test_segment);
  g_test_add_func ("/path/get_point", test_get_point);
  g_test_add_func ("/path/closest_point", test_closest_point);
  g_test_add_func ("/path/closest_point_for_point", test_closest_point_for_point);
  g_test_add_func ("/path/from-string", test_from_string);

  return g_test_run ();
}

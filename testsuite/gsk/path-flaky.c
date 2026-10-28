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

static GskPath *
create_random_degenerate_path (guint max_contours)
{
#define N_DEGENERATE_PATHS 15
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
      /* circle with radius 0 */
      gsk_path_builder_add_circle (builder,
                                   &GRAPHENE_POINT_INIT (g_test_rand_double_range (-1000, 1000),
                                                         g_test_rand_double_range (-1000, 1000)),
                                   0);
      break;

    case 13:
      /* a zero-length line */
      {
        graphene_point_t point = GRAPHENE_POINT_INIT (g_test_rand_double_range (-1000, 1000),
                                                      g_test_rand_double_range (-1000, 1000));
        gsk_path_builder_move_to (builder, point.x, point.y);
        gsk_path_builder_line_to (builder, point.x, point.y);
      }
      break;

    case 14:
      /* a cubic with start == end */
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
      switch (g_test_rand_int_range (0, 8))
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

        case 6:
          gsk_path_builder_conic_to (builder,
                                     g_test_rand_double_range (-1000, 1000),
                                     g_test_rand_double_range (-1000, 1000),
                                     g_test_rand_double_range (-1000, 1000),
                                     g_test_rand_double_range (-1000, 1000),
                                     g_test_rand_double_range (0.2, 20));
          break;

        case 7:
          gsk_path_builder_rel_conic_to (builder,
                                         g_test_rand_double_range (-1000, 1000),
                                         g_test_rand_double_range (-1000, 1000),
                                         g_test_rand_double_range (-1000, 1000),
                                         g_test_rand_double_range (-1000, 1000),
                                         g_test_rand_double_range (0.2, 20));
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
test_parse (void)
{
  int i;

  for (i = 0; i < 1000; i++)
    {
      GskPath *path1, *path2;
      char *string1, *string2;

      path1 = create_random_path (G_MAXUINT);
      string1 = gsk_path_to_string (path1);
      g_assert_nonnull (string1);

      path2 = gsk_path_parse (string1);
      g_assert_nonnull (path2);
      string2 = gsk_path_to_string (path2);
      g_assert_nonnull (string2);

      assert_path_equal_with_epsilon (path1, path2, 1.f / 1024);

      gsk_path_unref (path2);
      gsk_path_unref (path1);
      g_free (string2);
      g_free (string1);
    }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/create", test_create);
  g_test_add_func ("/path/parse", test_parse);

  return g_test_run ();
}

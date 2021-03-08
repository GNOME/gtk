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

#include "config.h"

#include <gtk/gtk.h>

static void
test_contains_rect (void)
{
  static double points[] = { -5, 0, 5, 10, 15, 85, 90, 95, 100, 105 };
#define LAST (G_N_ELEMENTS(points) - 1)
  GskRoundedRect rounded;
  guint x1, x2, y1, y2;

  gsk_rounded_rect_init_from_rect (&rounded, &GRAPHENE_RECT_INIT (0, 0, 100, 100), 10);

  for (x1 = 0; x1 < G_N_ELEMENTS (points); x1++)
    for (x2 = x1 + 1; x2 < G_N_ELEMENTS (points); x2++)
      for (y1 = 0; y1 < G_N_ELEMENTS (points); y1++)
        for (y2 = y1 + 1; y2 < G_N_ELEMENTS (points); y2++)
          {
            graphene_rect_t rect;
            gboolean inside;

            /* check all points are in the bounding box */
            inside = x1 > 0 && y1 > 0 && x2 < LAST && y2 < LAST;
            /* now check all the corners */
            inside &= x1 > 2 || y1 > 2 || (x1 == 2 && y1 == 2);
            inside &= x2 < LAST - 2 || y1 > 2 || (x2 == LAST - 2 && y1 == 2);
            inside &= x2 < LAST - 2 || y2 < LAST - 2 || (x2 == LAST - 2 && y2 == LAST - 2);
            inside &= x1 > 2 || y2 < LAST - 2 || (x1 == 2 && y2 == LAST - 2);

            graphene_rect_init (&rect, points[x1], points[y1], points[x2] - points[x1], points[y2] - points[y1]);
            if (inside)
              g_assert_true (gsk_rounded_rect_contains_rect (&rounded, &rect));
            else
              g_assert_false (gsk_rounded_rect_contains_rect (&rounded, &rect));
          }
#undef LAST
}

static void
test_intersects_rect (void)
{
  static double points[] = { -1, 0, 1, 99, 100, 101 };
#define ALL_THE_POINTS (G_N_ELEMENTS(points))
#define HALF_THE_POINTS (ALL_THE_POINTS / 2)
  GskRoundedRect rounded;
  guint x1, x2, y1, y2;

  gsk_rounded_rect_init_from_rect (&rounded, &GRAPHENE_RECT_INIT (0, 0, 100, 100), 10);

  for (x1 = 0; x1 < ALL_THE_POINTS; x1++)
    for (x2 = x1 + 1; x2 < ALL_THE_POINTS; x2++)
      for (y1 = 0; y1 < ALL_THE_POINTS; y1++)
        for (y2 = y1 + 1; y2 < ALL_THE_POINTS; y2++)
          {
            graphene_rect_t rect;
            gboolean should_contain_x, should_contain_y;

            graphene_rect_init (&rect, points[x1], points[y1], points[x2] - points[x1], points[y2] - points[y1]);
            should_contain_x = x1 < HALF_THE_POINTS && x2 >= HALF_THE_POINTS && y2 > 1 && y1 < ALL_THE_POINTS - 2;
            should_contain_y = y1 < HALF_THE_POINTS && y2 >= HALF_THE_POINTS && x2 > 1 && x1 < ALL_THE_POINTS - 2;
            if (should_contain_x || should_contain_y)
              g_assert_true (gsk_rounded_rect_intersects_rect (&rounded, &rect));
            else
              g_assert_false (gsk_rounded_rect_intersects_rect (&rounded, &rect));
          }
#undef ALL_THE_POINTS
#undef HALF_THE_POINTS
}

static void
test_contains_point (void)
{
  GskRoundedRect rect;

  gsk_rounded_rect_init (&rect,
                         &GRAPHENE_RECT_INIT (0, 0, 100, 100),
                         &GRAPHENE_SIZE_INIT (0, 0),
                         &GRAPHENE_SIZE_INIT (10, 10),
                         &GRAPHENE_SIZE_INIT (10, 20),
                         &GRAPHENE_SIZE_INIT (20, 10));

  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (50, 50)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (0, 0)));
  g_assert_false (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (100, 0)));
  g_assert_false (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (100, 100)));
  g_assert_false (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (0, 100)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (0, 50)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (50, 0)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (50, 100)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (100, 50)));

  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (95, 5)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (95, 90)));
  g_assert_true (gsk_rounded_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (10, 95)));
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/rounded-rect/contains-rect", test_contains_rect);
  g_test_add_func ("/rounded-rect/intersects-rect", test_intersects_rect);
  g_test_add_func ("/rounded-rect/contains-point", test_contains_point);

  return g_test_run ();
}

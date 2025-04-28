/*
 * Copyright © 2025 Red Hat, Inc.
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
#include "gsk/gskrectprivate.h"
#include "gsk/gskroundedrectprivate.h"

static void
test_snap_point (void)
{
  graphene_vec2_t scale;
  graphene_point_t offset;
  graphene_point_t src;
  graphene_point_t dest;

  graphene_vec2_init (&scale, 1, 1);
  graphene_point_init (&offset, 0, 0);

  graphene_point_init (&src, 1.5, 1.5);
  gsk_point_snap_to_grid (&src, GSK_POINT_SNAP_NONE, &scale, &offset, &dest);
  g_assert_true (graphene_point_equal (&dest, &src));

  gsk_point_snap_to_grid (&src, GSK_POINT_SNAP_INIT (GSK_SNAP_FLOOR, GSK_SNAP_CEIL), &scale, &offset, &dest);
  g_assert_true (graphene_point_equal (&dest, &GRAPHENE_POINT_INIT (1, 2)));

  gsk_point_snap_to_grid (&src, GSK_POINT_SNAP_INIT (GSK_SNAP_NONE, GSK_SNAP_ROUND), &scale, &offset, &dest);
  g_assert_true (graphene_point_equal (&dest, &GRAPHENE_POINT_INIT (1.5, 2)));

  graphene_vec2_init (&scale, 1.25, 1.25);
  graphene_point_init (&offset, 0.5, 0);

  gsk_point_snap_to_grid (&src, GSK_POINT_SNAP_INIT (GSK_SNAP_ROUND, GSK_SNAP_ROUND), &scale, &offset, &dest);
  g_assert_true (graphene_point_near (&dest, &GRAPHENE_POINT_INIT (1.9, 1.6), 1e-6));
}

static void
test_snap_rect (void)
{
  graphene_vec2_t scale;
  graphene_point_t offset;
  graphene_rect_t src;
  graphene_rect_t dest;
  graphene_point_t origin, opposite;

  graphene_vec2_init (&scale, 1, 1);
  graphene_point_init (&offset, 0, 0);

  graphene_rect_init (&src, 0.5, 0.333, 1, 2);
  gsk_rect_snap_to_grid (&src, GSK_RECT_SNAP_NONE, &scale, &offset, &dest);
  g_assert_true (graphene_rect_equal (&dest, &src));

  gsk_rect_snap_to_grid (&src, GSK_RECT_SNAP_GROW, &scale, &offset, &dest);
  g_assert_true (graphene_rect_equal (&dest, &GRAPHENE_RECT_INIT (0, 0, 2, 3)));

  graphene_vec2_init (&scale, 1.25, 1.25);
  graphene_point_init (&offset, 0, 0);

  graphene_rect_init (&src, 1.5, 2.5, 2, 3);
  gsk_rect_snap_to_grid (&src, GSK_RECT_SNAP_ROUND, &scale, &offset, &dest);

  graphene_rect_get_top_left (&dest, &origin);
  graphene_rect_get_bottom_right (&dest, &opposite);

  g_assert_true (graphene_point_near (&origin, &GRAPHENE_POINT_INIT (1.6, 2.4), 1e-6));
  g_assert_true (graphene_point_near (&opposite, &GRAPHENE_POINT_INIT (1.6 + 1.6, 2.4 + 3.2), 1e-6));
}

static void
test_snap_rounded_rect (void)
{
  graphene_vec2_t scale;
  graphene_point_t offset;
  GskRoundedRect src;
  GskRoundedRect dest;
  GskRoundedRect cmp;

  graphene_vec2_init (&scale, 1, 1);
  graphene_point_init (&offset, 0, 0);

  gsk_rounded_rect_init (&src,
                         &GRAPHENE_RECT_INIT (0.5, 0.333, 10, 20),
                         &GRAPHENE_SIZE_INIT (0, 0),
                         &GRAPHENE_SIZE_INIT (0.2, 1),
                         &GRAPHENE_SIZE_INIT (1, 1),
                         &GRAPHENE_SIZE_INIT (1.7, 0.4));

  gsk_rounded_rect_snap_to_grid (&src, GSK_RECT_SNAP_NONE, &scale, &offset, &dest);
  g_assert_true (gsk_rounded_rect_equal (&dest, &src));

  gsk_rounded_rect_init (&cmp,
                         &GRAPHENE_RECT_INIT (0, 0, 11, 21),
                         &GRAPHENE_SIZE_INIT (0, 0),
                         &GRAPHENE_SIZE_INIT (0.2, 1),
                         &GRAPHENE_SIZE_INIT (1, 1),
                         &GRAPHENE_SIZE_INIT (1.7, 0.4));
  gsk_rounded_rect_snap_to_grid (&src, GSK_RECT_SNAP_GROW, &scale, &offset, &dest);
  g_assert_true (gsk_rounded_rect_equal (&dest, &cmp));
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);
  g_test_set_nonfatal_assertions ();

  g_test_add_func ("/snap/point", test_snap_point);
  g_test_add_func ("/snap/rect", test_snap_rect);
  g_test_add_func ("/snap/rounded-rect", test_snap_rounded_rect);

  return g_test_run ();
}

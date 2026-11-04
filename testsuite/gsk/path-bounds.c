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

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/circle/bounds", test_circle_bounds);

  return g_test_run ();
}

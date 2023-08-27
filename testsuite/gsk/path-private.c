/*
 * Copyright © 2023 Red Hat, Inc.
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
#include "gsk/gskpathprivate.h"
#include "gsk/gskcontourprivate.h"

static void
test_roundtrip_circle (void)
{
  GskPathBuilder *builder;
  GskPath *path, *path2;
  const GskContour *contour;
  char *s;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (100, 100), 33);
  path = gsk_path_builder_free_to_path (builder);
  contour = gsk_path_get_contour (path, 0);

  g_assert_cmpstr (gsk_contour_get_type_name (contour), ==, "GskCircleContour");

  s = gsk_path_to_string (path);
  path2 = gsk_path_parse (s);
  contour = gsk_path_get_contour (path2, 0);

  g_assert_cmpstr (gsk_contour_get_type_name (contour), ==, "GskCircleContour");

  g_free (s);
  gsk_path_unref (path2);
  gsk_path_unref (path);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/path/roundtrip/circle", test_roundtrip_circle);

  return g_test_run ();
}

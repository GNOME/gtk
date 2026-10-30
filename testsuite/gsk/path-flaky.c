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

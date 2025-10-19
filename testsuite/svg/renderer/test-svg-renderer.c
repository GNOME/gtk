/*
 * Copyright (C) 2025 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtk/gtksvgprivate.h"
#include "testsuite/testutils.h"

static char *
test_get_reference_file (const char *svg_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (svg_file, ".svg"))
    g_string_append_len (file, svg_file, strlen (svg_file) - 4);
  else
    g_string_append (file, svg_file);

  g_string_append (file, ".nodes");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return g_strdup (svg_file);
    }

  return g_string_free (file, FALSE);
}

static void
render_svg_file (GFile *file, gboolean generate)
{
  GtkSvg *svg;
  char *svg_file, *reference_file;
  char *contents;
  size_t length;
  GBytes *bytes;
  char *diff;
  GError *error = NULL;
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  int64_t load_time;

  svg_file = g_file_get_path (file);

  if (!g_file_get_contents (svg_file, &contents, &length, &error))
    g_error ("%s", error->message);

  bytes = g_bytes_new_take (contents, length);
  svg = gtk_svg_new_from_bytes (bytes);
  g_bytes_unref (bytes);
  g_assert_no_error (error);

  load_time = g_get_monotonic_time ();
  gtk_svg_set_load_time (svg, load_time);
  gtk_svg_advance (svg, load_time);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (GDK_PAINTABLE (svg), snapshot, 100, 100);
  node = gtk_snapshot_free_to_node (snapshot);
  bytes = gsk_render_node_serialize (node);
  if (generate)
    {
      g_print ("%s", (const char *) g_bytes_get_data (bytes, NULL));
      goto out;
    }

  reference_file = test_get_reference_file (svg_file);

  diff = diff_bytes_with_file (reference_file, bytes, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_test_message ("Resulting file doesn't match reference:\n%s", diff);
      g_test_fail ();
    }
  g_free (reference_file);
  g_free (diff);

out:
  g_bytes_unref (bytes);
  g_free (svg_file);
  g_object_unref (svg);
  gsk_render_node_unref (node);
}

static void
test_svg_file (GFile *file)
{
  render_svg_file (file, FALSE);
}

static void
add_test_for_file (GFile *file)
{
  char *path;

  path = g_file_get_path (file);

  g_test_add_vtable (path,
                     0,
                     g_object_ref (file),
                     NULL,
                     (GTestFixtureFunc) test_svg_file,
                     (GTestFixtureFunc) g_object_unref);

  g_free (path);
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  char *path1, *path2;
  int result;

  path1 = g_file_get_path (file1);
  path2 = g_file_get_path (file2);

  result = strcmp (path1, path2);

  g_free (path1);
  g_free (path2);

  return result;
}

static void
add_tests_for_files_in_directory (GFile *dir)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".svg") ||
          g_str_has_suffix (filename, ".out.svg") ||
          g_str_has_suffix (filename, ".ref.svg"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (dir, filename));

      g_object_unref (info);
    }

  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  g_list_foreach (files, (GFunc) add_test_for_file, NULL);
  g_list_free_full (files, g_object_unref);
}

int
main (int argc, char **argv)
{
  if (argc >= 2 && strcmp (argv[1], "--generate") == 0)
    {
      GFile *file;

      gtk_init ();

      file = g_file_new_for_commandline_arg (argv[2]);
      render_svg_file (file, TRUE);
      g_object_unref (file);

      return 0;
    }

  gtk_test_init (&argc, &argv);

  if (argc < 2)
    {
      const char *basedir;
      GFile *dir;

      basedir = g_test_get_dir (G_TEST_DIST);
      dir = g_file_new_for_path (basedir);
      add_tests_for_files_in_directory (dir);

      g_object_unref (dir);
    }
  else
    {
      for (guint i = 1; i < argc; i++)
        {
          GFile *file;

          file = g_file_new_for_commandline_arg (argv[i]);
          add_test_for_file (file);
          g_object_unref (file);
        }
    }

  return g_test_run ();
}


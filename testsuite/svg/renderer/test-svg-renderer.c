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
test_get_sibling_file (const char *svg_file, const char *ext)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (svg_file, ".svg"))
    g_string_append_len (file, svg_file, strlen (svg_file) - 4);
  else
    g_string_append (file, svg_file);

  g_string_append (file, ext);

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return NULL;
    }

  return g_string_free (file, FALSE);
}

static void
add_error_context (const GError *error,
                   GString      *string)
{
  if (error->domain == GTK_SVG_ERROR)
    {
      const GtkSvgLocation *start = gtk_svg_error_get_start (error);
      const GtkSvgLocation *end = gtk_svg_error_get_end (error);
      const char *element = gtk_svg_error_get_element (error);
      const char *attribute = gtk_svg_error_get_attribute (error);

      if (start)
        {
          if (end->lines != start->lines || end->line_chars != start->line_chars)
            g_string_append_printf (string,
                                    "%lu.%lu - %lu.%lu: ",
                                    start->lines, start->line_chars,
                                    end->lines, end->line_chars);
          else
            g_string_append_printf (string,
                                    "%lu.%lu: ",
                                    start->lines, start->line_chars);
        }

      if (element && attribute)
        g_string_append_printf (string, "(%s / %s): ", element, attribute);
      else if (element)
        g_string_append_printf (string, "(%s): ", element);
    }
}

static void
error_cb (GtkSvg       *svg,
          const GError *error,
          GString      *errors)
{
  add_error_context (error,errors);

  if (error->domain == GTK_SVG_ERROR)
    {
      GEnumClass *class = g_type_class_get (GTK_TYPE_SVG_ERROR);
      GEnumValue *value = g_enum_get_value (class, error->code);
      g_string_append (errors, value->value_name);
    }
  else
    {
      g_string_append_printf (errors,
                              "%s %u",
                              g_quark_to_string (error->domain),
                              error->code);
    }

  g_string_append_c (errors, '\n');
}

static void
render_svg_file (GFile *file, gboolean generate)
{
  GtkSvg *svg;
  char *svg_file;
  char *reference_file;
  char *errors_file;
  char *contents;
  size_t length;
  GBytes *bytes;
  char *diff;
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  GString *errors;
  GError *error = NULL;

  errors = g_string_new ("");

  svg_file = g_file_get_path (file);

  if (!g_file_get_contents (svg_file, &contents, &length, &error))
    g_error ("%s", error->message);

  svg = gtk_svg_new ();
  g_signal_connect (svg, "error", G_CALLBACK (error_cb), errors);

  bytes = g_bytes_new_take (contents, length);
  gtk_svg_load_from_bytes (svg, bytes);
  g_clear_pointer (&bytes, g_bytes_unref);

  gtk_svg_play (svg);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (GDK_PAINTABLE (svg), snapshot, 100, 100);
  node = gtk_snapshot_free_to_node (snapshot);
  if (node)
    {
      bytes = gsk_render_node_serialize (node);
      gsk_render_node_unref (node);
    }
  else
    {
      bytes = g_bytes_new_static ("", 0);
    }

  if (generate)
    {
      g_print ("%s", (const char *) g_bytes_get_data (bytes, NULL));
      goto out;
    }

  reference_file = test_get_sibling_file (svg_file, ".nodes");

  diff = diff_bytes_with_file (reference_file, bytes, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_test_message ("Resulting file doesn't match reference:\n%s", diff);
      g_test_fail ();
    }
  g_free (reference_file);
  g_clear_pointer (&diff,g_free);

  errors_file = test_get_sibling_file (svg_file, ".errors");
  if (errors_file)
    {
      diff = diff_string_with_file (errors_file, errors->str, errors->len, &error);
      g_assert_no_error (error);

      if (diff && diff[0])
        {
          g_test_message ("Errors don't match expected errors:\n%s", diff);
          g_test_fail ();
        }
      g_free (diff);
    }
  else
    g_assert_true (errors->len == 0);

  g_free (errors_file);

out:
  g_string_free (errors, TRUE);
  g_free (svg_file);
  g_clear_pointer (&bytes, g_bytes_unref);
  g_object_unref (svg);
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


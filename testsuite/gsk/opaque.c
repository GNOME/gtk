/*
 * Copyright (C) 2024 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@redhat.com>
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

#include "config.h"

#include <gtk/gtk.h>

#include <errno.h>

static gboolean using_tap;

static gboolean
parse_float (const char *input,
             float      *out)
{
  char *s;
  float f;

  f = g_ascii_strtod (input, &s);

  if (errno == ERANGE || s == input || *s != 0 ||
      isinf (f) || isnan (f))
    return FALSE;

  *out = f;
  return TRUE;
}

static gboolean
parse_rect_from_filename (const char      *filename,
                          graphene_rect_t *out_rect)
{
  char **parts;
  gsize n;
  gboolean result;

  parts = g_strsplit (filename, "-", -1);
  n = g_strv_length (parts);
  if (g_str_has_suffix (parts[n-1], ".node"))
    parts[n-1][strlen(parts[n-1])-5] = 0;
  
  result = n > 4 &&
           parse_float (parts[n-4], &out_rect->origin.x) &&
           parse_float (parts[n-3], &out_rect->origin.y) &&
           parse_float (parts[n-2], &out_rect->size.width) &&
           parse_float (parts[n-1], &out_rect->size.height);

  g_strfreev (parts);

  return result;
}

static void
deserialize_error_func (const GskParseLocation *start,
                        const GskParseLocation *end,
                        const GError           *error,
                        gpointer                user_data)
{
  GString *string = g_string_new (user_data);

  g_string_append_printf (string, ":%zu:%zu",
                          start->lines + 1, start->line_chars + 1);
  if (start->lines != end->lines || start->line_chars != end->line_chars)
    {
      g_string_append (string, "-");
      if (start->lines != end->lines)
        g_string_append_printf (string, "%zu:", end->lines + 1);
      g_string_append_printf (string, "%zu", end->line_chars + 1);
    }

  g_test_message ("%s", string->str);
  g_string_free (string, TRUE);
  
  g_test_fail ();
}

static void
test_opaqueness (GFile *file)
{
  GskRenderNode *node;
  char *node_file;
  GBytes *bytes;
  GError *error = NULL;
  graphene_rect_t opaque, expected;
  gboolean is_opaque;

  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  if (error)
    {
      g_test_message ("Failed to load file: %s", error->message);
      g_clear_error (&error);
      g_test_fail ();
      return;
    }
  g_assert_nonnull (bytes);

  node = gsk_render_node_deserialize (bytes, deserialize_error_func, file);
  g_bytes_unref (bytes);
  is_opaque = gsk_render_node_get_opaque_rect (node, &opaque);

  node_file = g_file_get_path (file);
  if (parse_rect_from_filename (node_file, &expected))
    {
      if (is_opaque)
        {
          if (!graphene_rect_equal (&opaque, &expected))
            {
              g_test_message ("Should be %g %g %g %g but is %g %g %g %g",
                              expected.origin.x, expected.origin.y,
                              expected.size.width, expected.size.height,
                              opaque.origin.x, opaque.origin.y,
                              opaque.size.width, opaque.size.height);
              g_test_fail ();
            }
        }
      else
        {
          g_test_message ("Should be %g %g %g %g but is not opaque",
                          expected.origin.x, expected.origin.y,
                          expected.size.width, expected.size.height);
          g_test_fail ();
        }
    }
  else
    {
      if (is_opaque)
        {
          g_test_message ("Should not be opaque, but is %g %g %g %g",
                          opaque.origin.x, opaque.origin.y,
                          opaque.size.width, opaque.size.height);
          g_test_fail ();
        }
    }

  gsk_render_node_unref (node);
  g_free (node_file);
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
add_test_for_file (GFile *file)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;

  if (g_file_query_file_type (file, 0, NULL) != G_FILE_TYPE_DIRECTORY)
    {
      g_test_add_vtable (g_file_peek_path (file),
                         0,
                         g_object_ref (file),
                         NULL,
                         (GTestFixtureFunc) test_opaqueness,
                         (GTestFixtureFunc) g_object_unref);
      return;
    }

  enumerator = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".node"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (file, filename));

      g_object_unref (info);
    }

  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  g_list_foreach (files, (GFunc) add_test_for_file, NULL);
  g_list_free_full (files, g_object_unref);
}

static gboolean
parse_command_line (int *argc, char ***argv)
{
  int i;

  for (i = 0; i < *argc; i++)
    {
      if (strcmp ((*argv)[i], "--tap") == 0)
        using_tap = TRUE;
    }

  gtk_test_init (argc, argv);

  return TRUE;
}

int
main (int argc, char **argv)
{
  int result;

  if (!parse_command_line (&argc, &argv))
    return 1;

  if (argc < 2)
    {
      char *dirname;
      GFile *dir;

      dirname = g_build_filename (g_test_get_dir (G_TEST_DIST), "opaque", NULL);
      dir = g_file_new_for_path (dirname);
      g_free (dirname);

      add_test_for_file (dir);

      g_object_unref (dir);
    }
  else
    {
      guint i;

      for (i = 1; i < argc; i++)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[i]);

          add_test_for_file (file);

          g_object_unref (file);
        }
    }

  result = g_test_run ();

  if (using_tap)
    return 0;

  return result;
}


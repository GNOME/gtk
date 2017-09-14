/*
 * Copyright (C) 2017 Red Hat Inc.
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

#include <string.h>
#include <glib/gstdio.h>
#include <gsk/gsk.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
# include <io.h>
#endif

static char *
test_get_reference_file (const char *glsl_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (glsl_file, ".glsl"))
    g_string_append_len (file, glsl_file, strlen (glsl_file) - 4);
  else
    g_string_append (file, glsl_file);
  
  g_string_append (file, ".ref.glsl");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return g_strdup (glsl_file);
    }

  return g_string_free (file, FALSE);
}

static char *
test_get_errors_file (const char *glsl_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (glsl_file, ".glsl"))
    g_string_append_len (file, glsl_file, strlen (glsl_file) - 4);
  else
    g_string_append (file, glsl_file);
  
  g_string_append (file, ".errors");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return NULL;
    }

  return g_string_free (file, FALSE);
}

static char *
diff_with_file (const char  *file1,
                char        *text,
                gssize       len,
                GError     **error)
{
  const char *command[] = { "diff", "-u", file1, NULL, NULL };
  char *diff, *tmpfile;
  int fd;

  diff = NULL;

  if (len < 0)
    len = strlen (text);
  
  /* write the text buffer to a temporary file */
  fd = g_file_open_tmp (NULL, &tmpfile, error);
  if (fd < 0)
    return NULL;

  if (write (fd, text, len) != (int) len)
    {
      close (fd);
      g_set_error (error,
                   G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Could not write data to temporary file '%s'", tmpfile);
      goto done;
    }
  close (fd);
  command[3] = tmpfile;

  /* run diff command */
  g_spawn_sync (NULL, 
                (char **) command,
                NULL,
                G_SPAWN_SEARCH_PATH,
                NULL, NULL,
	        &diff,
                NULL, NULL,
                error);

done:
  g_unlink (tmpfile);
  g_free (tmpfile);

  return diff;
}

#if 0
static void
append_error_value (GString *string,
                    GType    enum_type,
                    guint    value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (enum_type);
  enum_value = g_enum_get_value (enum_class, value);

  g_string_append (string, enum_value->value_name);

  g_type_class_unref (enum_class);
}
#endif

static void
parsing_error_cb (GskPixelShader        *shader,
                  gboolean               fatal,
                  const GskCodeLocation *location,
                  const GError          *error,
                  gpointer               user_data)
{
  GString *errors = user_data;

  if (fatal)
    g_test_fail ();

  g_string_append_printf (errors,
                          "%zu:%zu: %s: ",
                          location->lines, location->line_chars,
                          fatal ? "ERROR" : "warning");

  g_string_append_printf (errors, 
                          "%s\n",
                          error->message);
}

static void
parse_glsl_file (GFile *file, gboolean generate)
{
  GskPixelShader *shader;
  GBytes *bytes;
  char *glsl, *diff;
  char *glsl_file, *reference_file, *errors_file;
  gsize length;
  GString *errors;
  GError *error = NULL;

  glsl_file = g_file_get_path (file);
  errors = g_string_new ("");

  g_file_load_contents (file, NULL,
                        &glsl, &length,
                        NULL, &error);
  g_assert_no_error (error);
  bytes = g_bytes_new_take (glsl, length);
  
  shader = gsk_pixel_shader_new_for_data (bytes,
                                          parsing_error_cb,
                                          errors);

  glsl = gsk_pixel_shader_to_string (shader);

  if (generate)
    {
      g_print ("%s", glsl);
      goto out;
    }

  reference_file = test_get_reference_file (glsl_file);

  diff = diff_with_file (reference_file, glsl, -1, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_test_message ("Resulting CSS doesn't match reference:\n%s", diff);
      g_test_fail ();
    }
  g_free (reference_file);

  errors_file = test_get_errors_file (glsl_file);

  if (errors_file)
    {
      diff = diff_with_file (errors_file, errors->str, errors->len, &error);
      g_assert_no_error (error);

      if (diff && diff[0])
        {
          g_test_message ("Errors don't match expected errors:\n%s", diff);
          g_test_fail ();
        }
    }
  else if (errors->str[0])
    {
      g_test_message ("Unexpected errors:\n%s", errors->str);
      g_test_fail ();
    }

  g_object_unref (shader);
  g_free (errors_file);
  g_string_free (errors, TRUE);

  g_free (diff);

out:
  g_free (glsl_file);
  g_free (glsl);
}

static void
test_glsl_file (GFile *file)
{
  parse_glsl_file (file, FALSE);
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
                     (GTestFixtureFunc) test_glsl_file,
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

      if (!g_str_has_suffix (filename, ".glsl") ||
          g_str_has_suffix (filename, ".out.glsl") ||
          g_str_has_suffix (filename, ".ref.glsl"))
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
  else if (strcmp (argv[1], "--generate") == 0)
    {
      if (argc >= 3)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[2]);

          parse_glsl_file (file, TRUE);

          g_object_unref (file);
        }
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

  return g_test_run ();
}


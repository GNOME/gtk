/*
 * Copyright (C) 2015 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
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

#ifdef G_OS_WIN32
# include <io.h>
#endif


static char *
test_get_reference_file (const char *ui_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (ui_file, ".ui"))
    g_string_append_len (file, ui_file, strlen (ui_file) - 3);
  else
    g_string_append (file, ui_file);
  
  g_string_append (file, ".nodes");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return g_strdup (ui_file);
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

static void
load_ui_file (GFile *file, gboolean generate)
{
  GtkBuilder *builder;
  GtkWidget *window;
  GtkStyleContext *context;
  char *output, *diff;
  char *ui_file, *reference_file;
  GError *error = NULL;

  ui_file = g_file_get_path (file);

  if (g_str_has_suffix (ui_file, ".rtl.ui"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
  else
    gtk_widget_set_default_direction (GTK_TEXT_DIR_LTR);

  builder = gtk_builder_new_from_file (ui_file);
  window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));

  g_assert (window != NULL);

  context = gtk_widget_get_style_context (window);

  output = gtk_style_context_to_string (context, GTK_STYLE_CONTEXT_PRINT_RECURSE);

  if (generate)
    {
      g_print ("%s", output);
      goto out;
    }

  reference_file = test_get_reference_file (ui_file);

  diff = diff_with_file (reference_file, output, -1, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_test_message ("Resulting output doesn't match reference:\n%s", diff);
      g_test_fail ();
    }
  g_free (reference_file);
  g_free (diff);

out:
  g_free (output);
  g_free (ui_file);
}

static void
test_ui_file (GFile *file)
{
  load_ui_file (file, FALSE);
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
                     (GTestFixtureFunc) test_ui_file,
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

      if (!g_str_has_suffix (filename, ".ui") ||
          g_str_has_suffix (filename, ".nodes"))
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
  g_setenv ("GTK_CSS_DEBUG", "1", TRUE);

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

          load_ui_file (file, TRUE);

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


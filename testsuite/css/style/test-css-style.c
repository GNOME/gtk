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

/* There shall be no other styles */
#define GTK_STYLE_PROVIDER_PRIORITY_FORCE G_MAXUINT

static char *
test_get_other_file (const char *ui_file, const char *extension)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (ui_file, ".ui"))
    g_string_append_len (file, ui_file, strlen (ui_file) - strlen (".ui"));
  else
    g_string_append (file, ui_file);

  g_string_append (file, extension);

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return NULL;
    }

  return g_string_free (file, FALSE);
}

static GBytes *
diff_with_file (const char  *file1,
                char        *text,
                gssize       len,
                GError     **error)
{
  GSubprocess *process;
  GBytes *input, *output;

  process = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE
                              | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                              error,
                              "diff", "-u", file1, "-", NULL);
  if (process == NULL)
    return NULL;

  input = g_bytes_new_static (text, len >= 0 ? len : strlen (text));
  if (!g_subprocess_communicate (process,
                                 input,
                                 NULL,
                                 &output,
                                 NULL,
                                 error))
    {
      g_object_unref (process);
      g_bytes_unref (input);
      return NULL;
    }

  if (!g_subprocess_get_successful (process) &&
      /* this is the condition when the files differ */
      !(g_subprocess_get_if_exited (process) && g_subprocess_get_exit_status (process) == 1))
    {
      g_clear_pointer (&output, g_bytes_unref);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "The `diff' process exited with error status %d",
                   g_subprocess_get_exit_status (process));
    }

  g_object_unref (process);
  g_bytes_unref (input);

  return output;
}

static char *
fixup_style_differences (const char *str)
{
  GRegex *regex;
  char *result;

  /* normalize differences that creep in from hard-to-control environmental influences */
  regex = g_regex_new ("[.]solid-csd", 0, 0, NULL);
  result = g_regex_replace_literal (regex, str, -1, 0, ".csd", 0, NULL);
  g_regex_unref (regex);

  return result;
}

static void
style_context_changed (GtkWidget *window, const char **output)
{
  GtkStyleContext *context;
  char *str;

  context = gtk_widget_get_style_context (window);

  str = gtk_style_context_to_string (context, GTK_STYLE_CONTEXT_PRINT_RECURSE |
                                              GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE);

  *output = fixup_style_differences (str);

  g_free (str);

  g_main_context_wakeup (NULL);
}

static void
load_ui_file (GFile *file, gboolean generate)
{
  GtkBuilder *builder;
  GtkWidget *window;
  char *output;
  GBytes *diff;
  char *ui_file, *css_file, *reference_file;
  GtkCssProvider *provider;
  GError *error = NULL;

  ui_file = g_file_get_path (file);

  css_file = test_get_other_file (ui_file, ".css");
  g_assert (css_file != NULL);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_path (provider, css_file);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_FORCE);

  builder = gtk_builder_new_from_file (ui_file);
  window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));

  g_assert (window != NULL);

  output = NULL;
  g_signal_connect (window, "map", G_CALLBACK (style_context_changed), &output);

  gtk_widget_show (window);

  while (!output)
    g_main_context_iteration (NULL, FALSE);

  if (generate)
    {
      g_print ("%s", output);
      goto out;
    }

  reference_file = test_get_other_file (ui_file, ".nodes");
  g_assert (reference_file != NULL);

  diff = diff_with_file (reference_file, output, -1, &error);
  g_assert_no_error (error);

  if (diff && g_bytes_get_size (diff) > 0)
    {
      g_test_message ("Resulting output doesn't match reference:\n%s", (const char *) g_bytes_get_data (diff, NULL));
      g_test_fail ();
    }
  g_free (reference_file);
  g_clear_pointer (&diff, g_bytes_unref);

out:
  gtk_style_context_remove_provider_for_display (gdk_display_get_default (),
                                                 GTK_STYLE_PROVIDER (provider));
  g_object_unref (provider);

  g_free (output);
  g_free (ui_file);
  g_free (css_file);
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
  g_setenv ("GTK_THEME", "Empty", TRUE);

  if (argc >= 3 && strcmp (argv[1], "--generate") == 0)
    {
      GFile *file = g_file_new_for_commandline_arg (argv[2]);

      gtk_init ();

      g_object_set (gtk_settings_get_default (), "gtk-font-name", "Sans", NULL);

      load_ui_file (file, TRUE);

      g_object_unref (file);

      return 0;
    }

  gtk_test_init (&argc, &argv);
  g_object_set (gtk_settings_get_default (), "gtk-font-name", "Sans", NULL);

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


/*
 * Copyright (C) 2011 Red Hat Inc.
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

#include "config.h"
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>

#ifdef G_OS_WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

static gchar **states;

static void
record_state_change (AtkObject   *accessible,
                     const gchar *state,
                     gboolean     set,
                     GString     *string)
{
  GtkWidget *w;
  const gchar *name;

  if (states)
    {
      gint i;

      for (i = 0; states[i]; i++)
        {
          if (strcmp (states[i], state) == 0)
            break;
        }
      if (states[i] == NULL)
        return;
    }

  w = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  name = gtk_buildable_get_name (GTK_BUILDABLE (w));
  g_string_append_printf (string, "%s %s %d\n", name, state, set); 
}

static gboolean
stop (gpointer data)
{
  GMainLoop *loop = data;

  g_main_loop_quit (loop);

  return G_SOURCE_CONTINUE;
}

static void
do_action (GtkBuilder *builder, const gchar *action, GString *string)
{
  gchar **parts;
  gint len;
  gint i;

  parts = g_strsplit (action, " ", 0);
  len = g_strv_length (parts);
  if (len > 0)
    {
      if (strcmp (parts[0], "record") == 0)
        {
          for (i = 1; i < len; i++)
            {
              GObject *o, *a;

              o = gtk_builder_get_object (builder, parts[i]);
              if (ATK_IS_OBJECT (o))
                a = o;
              else if (GTK_IS_WIDGET (o))
                a = G_OBJECT (gtk_widget_get_accessible (GTK_WIDGET (o)));
              else
                g_assert_not_reached (); 
              g_signal_connect (a, "state-change", G_CALLBACK (record_state_change), string);
            }
        }
      else if (strcmp (parts[0], "states") == 0)
        {
          g_strfreev (states);
          states = g_strdupv (parts);
        }
      else if (strcmp (parts[0], "destroy") == 0)
        {
          for (i = 1; i < len; i++)
            {
              GObject *o;

              o = gtk_builder_get_object (builder, parts[i]);
              gtk_widget_destroy (GTK_WIDGET (o));
            }
        }
      else if (strcmp (parts[0], "show") == 0)
        {
          GObject *o;

          o = gtk_builder_get_object (builder, parts[1]);
          gtk_widget_show_now (GTK_WIDGET (o));
        }
      else if (strcmp (parts[0], "focus") == 0)
        {
          GObject *o;

          o = gtk_builder_get_object (builder, parts[1]);
          gtk_widget_grab_focus (GTK_WIDGET (o));
        }
      else if (strcmp (parts[0], "wait") == 0)
        {
          GMainLoop *loop;
          gulong id;

          loop = g_main_loop_new (NULL, FALSE);
          id = g_timeout_add (1000, stop, loop);
          g_main_loop_run (loop);
          g_source_remove (id);
          g_main_loop_unref (loop);
        }
    }
  g_free (parts);
}

static void
record_events (const gchar *ui_file,
               const gchar *in_file,
               GString     *string)
{
  GtkBuilder *builder;
  GError *error;
  gchar *contents;
  gchar **actions;
  gint i, len;

  builder = gtk_builder_new ();
  error = NULL;
  gtk_builder_add_from_file (builder, ui_file, &error);
  g_assert_no_error (error);
  
  g_file_get_contents (in_file, &contents, NULL, &error);
  g_assert_no_error (error);
  actions = g_strsplit (contents, "\n", 0);

  len = g_strv_length (actions);
  for (i = 0; i < len; i++)
    do_action (builder, actions[i], string);

  g_object_unref (builder);

  g_free (contents);
  g_strfreev (actions);
}

static gchar *
get_test_file (const gchar *test_file,
               const gchar *extension,
               gboolean     must_exist)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (test_file, ".ui"))
    g_string_append_len (file, test_file, strlen (test_file) - strlen (".ui"));
  else
    g_string_append (file, test_file);

  g_string_append (file, extension);

  if (must_exist &&
      !g_file_test (file->str, G_FILE_TEST_EXISTS))
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

static void
test_ui_file (GFile *file)
{
  gchar *ui_file, *in_file, *out_file;
  GString *record;
  GError *error = NULL;

  ui_file = g_file_get_path (file);
  in_file = get_test_file (ui_file, ".in", TRUE);
  out_file = get_test_file (ui_file, ".out", TRUE);
  record = g_string_new ("");

  record_events (ui_file, in_file, record);

  if (out_file)
    {
      char *diff = diff_with_file (out_file, record->str, record->len, &error);
      g_assert_no_error (error);

      if (diff && diff[0])
        {
          g_printerr ("Contents don't match expected contents:\n%s", diff);
          g_test_fail ();
          g_free (diff);
        }
    }
  else if (record->str[0])
    {
      g_test_message ("Expected a reference file:\n%s", record->str);
      g_test_fail ();
    }

  g_string_free (record, TRUE);
  g_free (in_file);
  g_free (out_file);
  g_free (ui_file);
}

static void
add_test_for_file (GFile *file)
{
  g_test_add_vtable (g_file_get_path (file),
                     0,
                     g_object_ref (file),
                     (GTestFixtureFunc) NULL,
                     (GTestFixtureFunc) test_ui_file,
                     (GTestFixtureFunc) g_object_unref);
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

      if (!g_str_has_suffix (filename, ".ui"))
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

static char *arg_base_dir = NULL;

static const GOptionEntry test_args[] = {
  { "directory",        'd', 0, G_OPTION_ARG_FILENAME, &arg_base_dir,
    "Directory to run tests from", "DIR" },
  { NULL }
};

int
main (int argc, char *argv[])
{
  const gchar *basedir;
  GFile *dir;
  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("- run GTK state tests");
  g_option_context_add_main_entries (context, test_args, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      return FALSE;
    }

  g_option_context_free (context);

  gtk_test_init (&argc, &argv, NULL);

  if (arg_base_dir)
    basedir = arg_base_dir;
  else
    basedir = g_test_get_dir (G_TEST_DIST);

  dir = g_file_new_for_path (basedir);
  add_tests_for_files_in_directory (dir);
  g_object_unref (dir);

  return g_test_run ();
}


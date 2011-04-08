/*
 * Copyright (C) 2011 Red Hat Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

static char *
test_get_reference_file (const char *css_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (css_file, ".css"))
    g_string_append_len (file, css_file, strlen (css_file) - 4);
  else
    g_string_append (file, css_file);
  
  g_string_append (file, ".ref.css");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return g_strdup (css_file);
    }

  return g_string_free (file, FALSE);
}

static char *
test_get_errors_file (const char *css_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (css_file, ".css"))
    g_string_append_len (file, css_file, strlen (css_file) - 4);
  else
    g_string_append (file, css_file);
  
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

static void
parsing_error_cb (GtkCssProvider *provider,
                  const gchar     *path,
                  guint            line,
                  guint            position,
                  const GError *   error,
                  GString *        errors)
{
  char *basename;

  g_assert (path);
  g_assert (line > 0);

  basename = g_path_get_basename (path);

  g_string_append_printf (errors,
                          "%s:%u: error: %s %u\n",
                          basename, line,
                          g_quark_to_string (error->domain),
                          error->code);

  g_free (basename);
}

static void
test_css_file (GFile *file)
{
  GtkCssProvider *provider;
  char *css, *diff;
  char *css_file, *reference_file, *errors_file;
  GString *errors;
  GError *error = NULL;

  css_file = g_file_get_path (file);
  errors = g_string_new ("");

  provider = gtk_css_provider_new ();
  g_signal_connect (provider, 
                    "parsing-error",
                    G_CALLBACK (parsing_error_cb),
                    errors);
  gtk_css_provider_load_from_path (provider,
                                   css_file,
                                   NULL);

  css = gtk_css_provider_to_string (provider);

  reference_file = test_get_reference_file (css_file);

  diff = diff_with_file (reference_file, css, -1, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_test_message ("%s", diff);
      g_assert_not_reached ();
    }

  g_free (css);
  g_free (reference_file);

  errors_file = test_get_errors_file (css_file);

  if (errors_file)
    {
      diff = diff_with_file (errors_file, errors->str, errors->len, &error);
      g_assert_no_error (error);

      if (diff && diff[0])
        {
          g_test_message ("%s", diff);
          g_assert_not_reached ();
        }
    }
  else if (errors->str[0])
    {
      g_test_message ("Unexpected errors:\n%s", errors->str);
      g_assert_not_reached ();
    }

  g_free (errors_file);
  g_string_free (errors, TRUE);

  g_free (diff);
  g_free (css_file);
}

int
main (int argc, char **argv)
{
  const char *basedir;
  GError *error = NULL;
  GFile *dir;
  GFileEnumerator *enumerator;
  GFileInfo *info;

  gtk_test_init (&argc, &argv);

  if (g_getenv ("srcdir"))
    basedir = g_getenv ("srcdir");
  else
    basedir = ".";
    
  dir = g_file_new_for_path (basedir);
  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      GFile *file;
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".css") ||
          g_str_has_suffix (filename, ".out.css") ||
          g_str_has_suffix (filename, ".ref.css"))
        {
          g_object_unref (info);
          continue;
        }

      file = g_file_get_child (dir, filename);

      g_test_add_vtable (g_file_get_path (file),
                         0,
                         file,
                         NULL,
                         (GTestFixtureFunc) test_css_file,
                         (GTestFixtureFunc) g_object_unref);

      g_object_unref (info);
    }
  
  g_assert_no_error (error);
  g_object_unref (enumerator);

  return g_test_run ();
}


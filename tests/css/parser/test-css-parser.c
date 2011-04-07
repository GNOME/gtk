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
test_get_output_file (const char *css_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (css_file, ".css"))
    g_string_append_len (file, css_file, strlen (css_file) - 4);
  else
    g_string_append (file, css_file);
  
  g_string_append (file, ".out.css");

  return g_string_free (file, FALSE);
}

static char *
diff_files (const char *file1,
            const char *file2,
            GError **   error)
{
  const char *command[] = { "diff", "-u", file1, file2, NULL };
  char *diff;

  /* run diff command */
  if (!g_spawn_sync (NULL, (char **) command, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
	&diff, NULL, NULL, error)) {
    return NULL;
  }

  return diff;
}

static void
test_css_file (GFile *file)
{
  GtkCssProvider *provider;
  char *css, *diff;
  char *css_file, *output_file, *reference_file;
  GError *error = NULL;

  css_file = g_file_get_path (file);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_path (provider,
                                   css_file,
                                   &error);
  g_assert_no_error (error);

  css = gtk_css_provider_to_string (provider);
  output_file = test_get_output_file (css_file);

  g_file_set_contents (output_file, css, -1, &error);
  g_assert_no_error (error);

  g_free (css);

  reference_file = test_get_reference_file (css_file);

  diff = diff_files (reference_file, output_file, &error);
  g_assert_no_error (error);

  g_free (reference_file);
  g_free (output_file);

  if (diff && diff[0])
    {
      g_test_message ("%s", diff);
      g_assert_not_reached ();
    }

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


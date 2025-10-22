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
get_sibling (const char *file,
             char       *name)
{
  char *dir = g_path_get_dirname (file);
  char *res = g_build_filename (dir, g_strstrip (name), NULL);
  g_free (dir);
  return res;
}

typedef enum
{
  TIME,
  STATE,
  OUTPUT
} StepType;

typedef struct
{
  StepType type;
  int64_t time;
  unsigned int state;
  char *output;
} Step;

static void
clear_step (gpointer data)
{
  Step *step = data;

  if (step->type == OUTPUT)
    g_free (step->output);
}

static void
render_svg_file (GFile *file, gboolean generate)
{
  const char *filename;
  GtkSvg *svg;
  char *svg_file;
  char *contents;
  size_t length;
  GBytes *bytes;
  GError *error = NULL;
  int64_t load_time;
  GArray *steps;

  steps = g_array_new (FALSE, FALSE, sizeof (Step));
  g_array_set_clear_func (steps, clear_step);

  filename = g_file_peek_path (file);

  if (g_str_has_suffix (filename, ".test"))
    {
      GStrv strv;

      if (!g_file_get_contents (filename, &contents, &length, &error))
        g_error ("%s", error->message);

      strv = g_strsplit (contents, "\n", 0);

      if (!g_str_has_prefix (strv[0], "input: "))
        g_error ("Can't parse %s\n", filename);

      svg_file = get_sibling (filename, strv[0] + strlen ("input: "));

      for (unsigned int i = 1; strv[i]; i++)
        {
          Step step;

          memset (&step, 0, sizeof (Step));

          if (g_str_has_prefix (strv[i], "state: "))
            {
              char *end;
              step.type = STATE;
              if (strcmp (strv[i] + strlen ("state: "), "empty") == 0)
                step.state = GTK_SVG_STATE_EMPTY;
              else
                {
                  step.state = g_ascii_strtoull (strv[i] + strlen ("state: "), &end, 10);
                  if ((end && *end != '\0') || step.state > 63)
                    g_error ("Can't parse %s\n", filename);
                }
              g_array_append_val (steps, step);
            }
          else if (g_str_has_prefix (strv[i], "time: "))
            {
              char *end;
              step.type = TIME;
              step.time = g_ascii_strtoull (strv[i] + strlen ("time: "), &end, 10);
              if (end && *end != '\0')
                g_error ("Can't parse %s\n", filename);
              g_array_append_val (steps, step);
            }
          else if (g_str_has_prefix (strv[i], "output: "))
            {
              step.type = OUTPUT;
              step.output = get_sibling (filename, strv[i] + strlen ("output: "));
              g_array_append_val (steps, step);
            }
        }

      g_strfreev (strv);
      g_free (contents);
    }
  else
    {
      Step step;
      char *p, *end = NULL;
      int64_t time;

      svg_file = g_file_get_path (file);
      p = strrchr (svg_file, '.');
      time = g_ascii_strtoull (&p[1], &end, 10);
      *p = '\0';

      step.type = TIME;
      step.time = time;
      g_array_append_val (steps, step);

      step.type = OUTPUT;
      step.output = g_file_get_path (file);
      g_array_append_val (steps, step);
    }

  if (!g_file_get_contents (svg_file, &contents, &length, &error))
    g_error ("%s", error->message);

  bytes = g_bytes_new_take (contents, length);
  svg = gtk_svg_new_from_bytes (bytes);
  g_bytes_unref (bytes);

  load_time = g_get_monotonic_time ();
  gtk_svg_set_load_time (svg, load_time);

  for (unsigned int i = 0; i < steps->len; i++)
    {
      Step *step = &g_array_index (steps, Step, i);
      switch (step->type)
        {
        case TIME:
          gtk_svg_advance (svg, load_time + step->time * G_TIME_SPAN_MILLISECOND);
          break;
        case STATE:
          gtk_svg_set_state (svg, step->state);
          break;
        case OUTPUT:
          {
            GBytes *output;

            output = gtk_svg_serialize_full (svg,
                                             GTK_SVG_SERIALIZE_AT_CURRENT_TIME |
                                             GTK_SVG_SERIALIZE_INCLUDE_STATE);
            if (generate)
              {
                if (!g_file_set_contents (step->output,
                                          g_bytes_get_data (output, NULL),
                                          g_bytes_get_size (output),
                                          &error))
                  g_error ("%s", error->message);
                g_print ("%s written\n", step->output);
              }
            else
              {
                char *diff;

                diff = diff_bytes_with_file (step->output, output, &error);
                g_assert_no_error (error);
                if (diff && diff[0])
                  {
                    g_test_message ("Resulting file doesn't match reference:\n%s", diff);
                    g_test_fail ();
                 }
                g_free (diff);
              }
            g_clear_pointer (&output, g_bytes_unref);
          }
          break;

        default:
          g_assert_not_reached ();
        }
    }

  g_array_unref (steps);
  g_free (svg_file);
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
      const char *filename = g_file_info_get_name (info);

      if (g_str_has_suffix (filename, ".test") ||
          g_regex_match_simple (".*\\.svg\\.[0-9]+$", filename, 0, 0))
        {
          g_print ("adding %s\n", filename);
          files = g_list_prepend (files, g_file_get_child (dir, filename));
        }

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
  if (argc >= 3 && strcmp (argv[1], "--generate") == 0)
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


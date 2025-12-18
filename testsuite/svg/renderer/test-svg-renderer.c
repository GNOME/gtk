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
file_replace_extension (const char *old_file,
                        const char *old_ext,
                        const char *new_ext)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (old_file, old_ext))
    g_string_append_len (file, old_file, strlen (old_file) - strlen (old_ext));
  else
    g_string_append (file, old_file);

  g_string_append (file, new_ext);

  return g_string_free (file, FALSE);
}

static char *
test_get_sibling_file (const char *svg_file, const char *ext)
{
  char *file;

  file = file_replace_extension (svg_file, ".svg", ext);

  if (!g_file_test (file, G_FILE_TEST_EXISTS))
    {
      g_free (file);
      return NULL;
    }

  return file;
}

static const char *arg_output_dir;

static const char *
get_output_dir (void)
{
  static const char *output_dir = NULL;
  GError *error = NULL;
  GFile *file;

  if (output_dir)
    return output_dir;

  if (arg_output_dir)
    output_dir = arg_output_dir;
  else
    output_dir = g_get_tmp_dir ();

  /* Just try to create the output directory.
   * If it already exists, that's exactly what we wanted to check,
   * so we can happily skip that error.
   */
  file = g_file_new_for_path (output_dir);
  if (!g_file_make_directory_with_parents (file, NULL, &error))
    {
      g_object_unref (file);

      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          g_error ("Failed to create output dir: %s", error->message);
          g_error_free (error);
          return NULL;
        }
      g_error_free (error);
    }
  else
    g_object_unref (file);

  return output_dir;
}

static char *
get_output_file (const char *file,
                 const char *extension)
{
  const char *dir;
  char *result, *base;
  char *name;

  dir = get_output_dir ();
  base = g_path_get_basename (file);
  name = file_replace_extension (base, ".svg", extension);
  result = g_strconcat (dir, G_DIR_SEPARATOR_S, name, NULL);

  g_free (base);
  g_free (name);

  return result;
}

static void
save_output (const char *contents,
             const char *input_file,
             const char *extension)
{
  char *filename = get_output_file (input_file, extension);
  gboolean result;

  g_print ("Storing test output at %s\n", filename);
  result = g_file_set_contents (filename, contents, strlen (contents), NULL);
  g_assert_true (result);
  g_free (filename);
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
                                    "%" G_GSIZE_FORMAT ".%" G_GSIZE_FORMAT " - %" G_GSIZE_FORMAT ".%" G_GSIZE_FORMAT ": ",
                                    start->lines, start->line_chars,
                                    end->lines, end->line_chars);
          else
            g_string_append_printf (string,
                                    "%" G_GSIZE_FORMAT ".%" G_GSIZE_FORMAT ": ",
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
  gdk_paintable_snapshot (GDK_PAINTABLE (svg), snapshot,
                          gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (svg)),
                          gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (svg)));
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

  reference_file = test_get_sibling_file (svg_file, ".node");

  diff = diff_bytes_with_file (reference_file, bytes, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_test_message ("Resulting file doesn't match reference:\n%s", diff);
      g_test_fail ();
    }

  if (diff || g_test_verbose ())
    {
       save_output (g_bytes_get_data (bytes, NULL), svg_file, ".out.node");
       save_output (diff, svg_file, ".node.diff");
    }

  g_free (reference_file);
  g_clear_pointer (&diff,g_free);

  errors_file = test_get_sibling_file (svg_file, ".errors");
  if (errors_file == NULL)
    errors_file = g_strdup ("/dev/null");

  if (errors_file)
    {
      diff = diff_string_with_file (errors_file, errors->str, errors->len, &error);
      g_assert_no_error (error);

      if (diff && diff[0])
        {
          g_test_message ("Errors don't match expected errors:\n%s", diff);
          g_test_fail ();
        }

      if (diff || g_test_verbose ())
        {
           save_output (errors->str, svg_file, ".out.errors");
           save_output (diff, svg_file, ".errors.diff");
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
  GOptionEntry options[] = {
    { "output", 0, 0, G_OPTION_ARG_FILENAME, &arg_output_dir, "Directory to save image files to", "DIR" },
    { NULL }
  };
  GOptionContext *context;
  GError *error = NULL;
  const char *srcdir = g_getenv ("G_TEST_SRCDIR");
  const char *static_fonts[] = {
    "SVGFreeSans.ttf",
    "FreeSerif.otf",
    "FreeSerifItalic.otf",
    "FreeSerifBold.otf",
    "FreeSerifBoldItalic.otf"
  };

  if (srcdir)
    for (size_t i = 0; i < G_N_ELEMENTS (static_fonts); i++)
      {
        char *fontpath = g_build_path (G_DIR_SEPARATOR_S, srcdir, "resources", static_fonts[i], NULL);
        if (!pango_font_map_add_font_file (pango_cairo_font_map_get_default (), fontpath, &error))
          {
            g_warning ("Failed to load %s: %s", static_fonts[i], error->message);
            g_clear_error (&error);
          }
        g_free (fontpath);
      }

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

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return 1;
    }
  else if (argc != 3 && argc != 2)
    {
      char *help = g_option_context_get_help (context, TRUE, NULL);
      g_print ("%s", help);
      return 1;
    }

  g_option_context_free (context);

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


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

typedef struct
{
  GFile *input;
  GFile *reference;
  GtkSvgFeatures features;
  GdkRGBA colors[5];
  size_t n_colors;
  double weight;

  gboolean generate;
} TestData;

static void
init_test_data (TestData *data)
{
  data->input = NULL;
  data->reference = NULL;
  data->features = GTK_SVG_EXTERNAL_RESOURCES |
                   GTK_SVG_EXTENSIONS |
                   GTK_SVG_ANIMATIONS;
  data->n_colors = 0;
  data->weight = -1;
  data->generate = FALSE;
}

static void
clear_test_data (TestData *data)
{
  g_object_unref (data->input);
  g_object_unref (data->reference);
}

static char *
filename_replace_extension (const char *old_file,
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

static GFile *
file_replace_extension (GFile      *file,
                        const char *old_ext,
                        const char *new_ext)
{
  char *path = filename_replace_extension (g_file_peek_path (file), old_ext, new_ext);
  GFile *new_file = g_file_new_for_path (path);
  g_free (path);
  return new_file;
}

static char *
test_get_sibling_file (const char *file, const char *fext, const char *sext)
{
  char *sfile;

  sfile = filename_replace_extension (file, fext, sext);

  if (!g_file_test (sfile, G_FILE_TEST_EXISTS))
    {
      g_free (sfile);
      return NULL;
    }

  return sfile;
}

static GFile *
test_get_sibling (GFile      *file,
                  const char *name)
{
  GFile *dir = g_file_get_parent (file);
  GFile *sibling = g_file_get_child (dir, name);
  g_object_unref (dir);
  return sibling;
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
                 const char *fext,
                 const char *extension,
                 gboolean is_compressed)
{
  const char *dir;
  char *result, *base;
  char *name;

  dir = get_output_dir ();
  base = g_path_get_basename (file);
  name = filename_replace_extension (base, fext, extension);
  result = g_strconcat (dir, G_DIR_SEPARATOR_S, name, is_compressed ? ".gz" : NULL, NULL);

  g_free (base);
  g_free (name);

  return result;
}

static void
save_output (const char *contents,
             const char *input_file,
             const char *input_file_ext,
             const char *extension,
             gboolean is_compressed)
{
  char *filename = get_output_file (input_file, input_file_ext, extension, is_compressed);
  gboolean result;

  if (is_compressed)
    {
      GConverter *compressor;
      GBytes *decompressed, *compressed;
      GError *err = NULL;
      gpointer data;
      gsize len;

      compressor = G_CONVERTER (g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, 8));
      decompressed = g_bytes_new (contents, strlen(contents));
      compressed = g_converter_convert_bytes (compressor, decompressed, &err);
      g_object_unref (compressor);
      g_bytes_unref (decompressed);
      if (err)
        g_error ("%s", err->message);
      data = g_bytes_unref_to_data (compressed, &len);

      g_print ("Storing compressed test output at %s\n", filename);
      result = g_file_set_contents (filename, data, len, NULL);
      g_free (data);
      g_assert_true (result);
    }
  else
    {
      g_print ("Storing test output at %s\n", filename);
      result = g_file_set_contents (filename, contents, strlen (contents), NULL);
      g_assert_true (result);
    }
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
render_svg_file (TestData *data)
{
  gboolean compressed;
  gboolean compressed_ref;
  GtkSvg *svg;
  char *svg_file;
  const char *file_ext;
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

  svg_file = g_file_get_path (data->input);
  compressed = g_str_has_suffix (svg_file, ".gz");
  if (compressed)
    file_ext = ".svg.gz";
  else
    file_ext = ".svg";

  compressed_ref = g_str_has_suffix (g_file_peek_path (data->reference), ".gz");

  if (!g_file_get_contents (svg_file, &contents, &length, &error))
    g_error ("%s", error->message);

  if (compressed)
    {
      GBytes *tmp;
      GConverter *decompressor;
      decompressor = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
      tmp = g_bytes_new_take (contents, length);
      bytes = g_converter_convert_bytes (decompressor, tmp, &error);
      g_object_unref (decompressor);
      g_bytes_unref (tmp);
      if (error)
        g_error ("%s", error->message);
    }
  else
    {
      bytes = g_bytes_new_take (contents, length);
    }

  svg = gtk_svg_new ();
  g_signal_connect (svg, "error", G_CALLBACK (error_cb), errors);

  gtk_svg_set_features (svg, data->features);
  gtk_svg_set_weight (svg, data->weight);

  gtk_svg_load_from_bytes (svg, bytes);
  g_clear_pointer (&bytes, g_bytes_unref);

  gtk_svg_play (svg);

  snapshot = gtk_snapshot_new ();

  if (data->n_colors > 0)
    gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (svg), snapshot,
                                              gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (svg)),
                                              gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (svg)),
                                              data->colors,
                                              data->n_colors);
  else
    gdk_paintable_snapshot (GDK_PAINTABLE (svg), snapshot,
                            gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (svg)),
                            gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (svg)));

  node = gtk_snapshot_free_to_node (snapshot);

  if (node)
    bytes = gsk_render_node_serialize (node);
  else
    bytes = g_bytes_new_static ("", 0);

  if (data->generate)
    {
      g_file_set_contents (g_file_peek_path (data->reference),
                           g_bytes_get_data (bytes, NULL),
                           g_bytes_get_size (bytes),
                           NULL);

      if (errors->len > 0)
        {
          errors_file = filename_replace_extension (svg_file, file_ext, ".errors");
          g_file_set_contents (errors_file, errors->str, errors->len, NULL);
          g_free (errors_file);
        }
      goto out;
    }

  diff = diff_node_with_file (g_file_peek_path (data->reference), node, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_test_message ("Resulting file doesn't match reference:\n%s", diff);
      g_test_fail ();
    }

  if (diff || g_test_verbose ())
    save_output (g_bytes_get_data (bytes, NULL), svg_file, file_ext, ".out.node", compressed_ref);
  if (diff)
    save_output (diff, svg_file, file_ext, ".node.diff", FALSE);

  g_clear_pointer (&diff, g_free);

  errors_file = test_get_sibling_file (svg_file, file_ext, ".errors");
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
        save_output (errors->str, svg_file, file_ext, ".out.errors", FALSE);
      if (diff)
        save_output (diff, svg_file, file_ext, ".errors.diff", FALSE);

      g_free (diff);
    }
  else
    g_assert_true (errors->len == 0);

  g_free (errors_file);

out:
  g_string_free (errors, TRUE);
  g_free (svg_file);
  g_object_unref (svg);
  g_clear_pointer (&bytes, g_bytes_unref);
  g_clear_pointer (&node, gsk_render_node_unref);
}

static void
read_test_data (GFile    *file,
                TestData *data)
{
  GError *err = NULL;
  GKeyFile *args;
  char *input;
  char *reference;
  gboolean animations;
  gboolean symbolic;
  char *string;

  animations = TRUE;
  symbolic = FALSE;

  args = g_key_file_new ();
  g_key_file_load_from_file (args, g_file_peek_path (file), G_KEY_FILE_NONE, &err);
  g_assert_no_error (err);

  input = g_key_file_get_string (args, "test", "input", &err);
  g_assert_no_error (err);
  data->input = test_get_sibling (file, input);

  reference = g_key_file_get_string (args, "test", "reference", &err);
  g_assert_no_error (err);
  data->reference = test_get_sibling (file, reference);

  animations = g_key_file_get_boolean (args, "test", "animations", &err);
  if (g_error_matches (err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
    g_clear_error (&err);

  symbolic = g_key_file_get_boolean (args, "test", "symbolic", &err);
  if (g_error_matches (err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
    g_clear_error (&err);

  string = g_key_file_get_string (args, "test", "colors", &err);
  if (g_error_matches (err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
    g_clear_error (&err);
  else
    {
      GStrv strv = g_strsplit (string, ",", 0);
      data->n_colors = g_strv_length (strv);
      for (unsigned int i = 0; i < data->n_colors; i++)
        {
          if (!gdk_rgba_parse (&data->colors[i], strv[i]))
            {
              data->n_colors = 0;
              break;
            }
        }
      g_strfreev (strv);
    }

  data->weight = g_key_file_get_double (args, "test", "weight", &err);
  if (g_error_matches (err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
    g_clear_error (&err);

  g_key_file_unref (args);

  data->features = GTK_SVG_EXTERNAL_RESOURCES |
                   GTK_SVG_EXTENSIONS |
                   (animations ? GTK_SVG_ANIMATIONS : 0) |
                   (symbolic ? GTK_SVG_TRADITIONAL_SYMBOLIC : 0);

  g_free (input);
  g_free (reference);
  g_free (string);
}

static void
test_data_for_svg (GFile *file, TestData *data)
{
  data->input = g_object_ref (file);
  data->reference = file_replace_extension (file, ".svg", ".node");
}

static void
test_file (GFile *file)
{
  TestData data;

  init_test_data (&data);

  if (g_str_has_suffix (g_file_peek_path (file), ".test"))
    read_test_data (file, &data);
  else
    test_data_for_svg (file, &data);

  data.generate = FALSE;

  render_svg_file (&data);

  clear_test_data (&data);
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
                     (GTestFixtureFunc) test_file,
                     (GTestFixtureFunc) g_object_unref);

  g_free (path);
}

int
main (int argc, char **argv)
{
  char *generate = NULL;
  GOptionEntry options[] = {
    { "output", 0, 0, G_OPTION_ARG_FILENAME, &arg_output_dir, "Directory to save image files to", "DIR" },
    { "generate", 0, 0, G_OPTION_ARG_FILENAME, &generate, "Generate", "FILE" },
    { NULL }
  };
  GOptionContext *context;
  GError *error = NULL;

  g_setenv ("GSK_SUBSET_FONTS", "1", TRUE);

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  g_option_context_free (context);

  if (generate)
    {
      GFile *file;
      TestData data;

      gtk_init ();

      file = g_file_new_for_commandline_arg (generate);

      init_test_data (&data);

      if (g_str_has_suffix (g_file_peek_path (file), ".test"))
        read_test_data (file, &data);
      else
        test_data_for_svg (file, &data);

      data.generate = TRUE;

      render_svg_file (&data);

      g_object_unref (file);

      return 0;
    }

  gtk_test_init (&argc, &argv);

  for (guint i = 1; i < argc; i++)
    {
      GFile *file;

      file = g_file_new_for_commandline_arg (argv[i]);
      add_test_for_file (file);
      g_object_unref (file);
    }

  return g_test_run ();
}

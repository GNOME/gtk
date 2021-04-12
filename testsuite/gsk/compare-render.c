#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include "../reftests/reftest-compare.h"

static char *arg_output_dir = NULL;

static const char *
get_output_dir (void)
{
  static const char *output_dir = NULL;
  GError *error = NULL;
  GFile *file;

  if (output_dir)
    return output_dir;

  if (arg_output_dir)
    {
      GFile *arg_file = g_file_new_for_commandline_arg (arg_output_dir);
      const char *subdir;

      subdir = g_getenv ("TEST_OUTPUT_SUBDIR");
      if (subdir)
        {
          GFile *child = g_file_get_child (arg_file, subdir);
          g_object_unref (arg_file);
          arg_file = child;
        }

      output_dir = g_file_get_path (arg_file);
      g_object_unref (arg_file);
    }
  else
    {
      output_dir = g_get_tmp_dir ();
    }

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
get_output_file (const char *file,
                 const char *orig_ext,
                 const char *new_ext)
{
  const char *dir;
  char *result, *base;
  char *name;

  dir = get_output_dir ();
  base = g_path_get_basename (file);
  name = file_replace_extension (base, orig_ext, new_ext);

  result = g_strconcat (dir, G_DIR_SEPARATOR_S, name, NULL);

  g_free (base);
  g_free (name);

  return result;
}

static void
save_image (cairo_surface_t *surface,
            const char      *test_name,
            const char      *extension)
{
  char *filename = get_output_file (test_name, ".node", extension);
  int status;

  g_print ("Storing test result image at %s\n", filename);
  status = cairo_surface_write_to_png (surface, filename);
  g_assert_true (status == CAIRO_STATUS_SUCCESS);
  g_free (filename);
}

static void
deserialize_error_func (const GskParseLocation *start,
                        const GskParseLocation *end,
                        const GError           *error,
                        gpointer                user_data)
{
  GString *string = g_string_new ("<data>");

  g_string_append_printf (string, ":%zu:%zu",
                          start->lines + 1, start->line_chars + 1);
  if (start->lines != end->lines || start->line_chars != end->line_chars)
    {
      g_string_append (string, "-");
      if (start->lines != end->lines)
        g_string_append_printf (string, "%zu:", end->lines + 1);
      g_string_append_printf (string, "%zu", end->line_chars + 1);
    }

  g_warning ("Error at %s: %s", string->str, error->message);

  g_string_free (string, TRUE);
}

static const GOptionEntry options[] = {
  { "output", 0, 0, G_OPTION_ARG_FILENAME, &arg_output_dir,
    "Directory to save image files to", "DIR" },
  { NULL }
};

/*
 * Non-option arguments:
 *   1) .node file to compare
 *   2) .png file to compare the rendered .node file to
 */
int
main (int argc, char **argv)
{
  cairo_surface_t *reference_surface = NULL;
  cairo_surface_t *rendered_surface = NULL;
  cairo_surface_t *diff_surface = NULL;
  GdkTexture *texture;
  GskRenderer *renderer;
  GdkSurface *window;
  GskRenderNode *node;
  const char *node_file;
  const char *png_file;
  gboolean success = TRUE;
  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("NODE REF - run GSK node tests");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return 1;
    }
  else if (argc != 3)
    {
      char *help = g_option_context_get_help (context, TRUE, NULL);
      g_print ("%s", help);
      return 1;
    }

  g_option_context_free (context);

  gtk_init ();

  node_file = argv[1];
  png_file = argv[2];

  window = gdk_surface_new_toplevel (gdk_display_get_default());
  renderer = gsk_renderer_new_for_surface (window);

  g_print ("Node file: '%s'\n", node_file);
  g_print ("PNG file: '%s'\n", png_file);

  /* Load the render node from the given .node file */
  {
    GBytes *bytes;
    gsize len;
    char *contents;

    if (!g_file_get_contents (node_file, &contents, &len, &error))
      {
        g_print ("Could not open node file: %s\n", error->message);
        g_clear_error (&error);
        return 1;
      }

    bytes = g_bytes_new_take (contents, len);
    node = gsk_render_node_deserialize (bytes, deserialize_error_func, &success);
    g_bytes_unref (bytes);

    g_assert_no_error (error);
    g_assert_nonnull (node);
  }

  /* Render the .node file and download to cairo surface */
  texture = gsk_renderer_render_texture (renderer, node, NULL);
  g_assert_nonnull (texture);

  rendered_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                 gdk_texture_get_width (texture),
                                                 gdk_texture_get_height (texture));
  gdk_texture_download (texture,
                        cairo_image_surface_get_data (rendered_surface),
                        cairo_image_surface_get_stride (rendered_surface));
  cairo_surface_mark_dirty (rendered_surface);

  /* Load the given reference png file */
  reference_surface = cairo_image_surface_create_from_png (png_file);
  if (cairo_surface_status (reference_surface))
    {
      g_print ("Error loading reference surface: %s\n",
               cairo_status_to_string (cairo_surface_status (reference_surface)));
      success = FALSE;
    }
  else
    {
      /* Now compare the two */
      diff_surface = reftest_compare_surfaces (rendered_surface, reference_surface);

      if (diff_surface)
        {
          save_image (diff_surface, node_file, ".diff.png");
          cairo_surface_destroy (diff_surface);
          success = FALSE;
        }
    }

  save_image (rendered_surface, node_file, ".out.png");

  cairo_surface_destroy (reference_surface);
  cairo_surface_destroy (rendered_surface);
  g_object_unref (texture);

  gsk_render_node_unref (node);

  return success ? 0 : 1;
}

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include "../reftests/reftest-compare.h"

static char *arg_output_dir = NULL;
static gboolean flip = FALSE;
static gboolean rotate = FALSE;
static gboolean repeat = FALSE;

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
save_image (GdkTexture *texture,
            const char *test_name,
            const char *extension)
{
  char *filename = get_output_file (test_name, ".node", extension);
  gboolean result;

  g_print ("Storing test result image at %s\n", filename);
  result = gdk_texture_save_to_png (texture, filename);
  g_assert_true (result);
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
  { "output", 0, 0, G_OPTION_ARG_FILENAME, &arg_output_dir, "Directory to save image files to", "DIR" },
  { "flip", 0, 0, G_OPTION_ARG_NONE, &flip, "Do flipped test", NULL },
  { "rotate", 0, 0, G_OPTION_ARG_NONE, &rotate, "Do rotated test", NULL },
  { "repeat", 0, 0, G_OPTION_ARG_NONE, &repeat, "Do repeat test", NULL },
  { NULL }
};

static GskRenderNode *
load_node_file (const char *node_file)
{
  GBytes *bytes;
  gsize len;
  char *contents;
  GError *error = NULL;
  GskRenderNode *node;

  if (!g_file_get_contents (node_file, &contents, &len, &error))
    {
      g_print ("Could not open node file: %s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }

  bytes = g_bytes_new_take (contents, len);
  node = gsk_render_node_deserialize (bytes, deserialize_error_func, NULL);
  g_bytes_unref (bytes);

  g_assert_no_error (error);
  g_assert_nonnull (node);

  return node;
}

/*
 * Non-option arguments:
 *   1) .node file to compare
 *   2) .png file to compare the rendered .node file to
 */
int
main (int argc, char **argv)
{
  GdkTexture *reference_texture;
  GdkTexture *rendered_texture;
  GskRenderer *renderer;
  GdkSurface *window;
  GskRenderNode *node;
  const char *node_file;
  const char *png_file;
  gboolean success = TRUE;
  GError *error = NULL;
  GOptionContext *context;
  GdkTexture *diff_texture;

  (g_test_init) (&argc, &argv, NULL);

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

  g_print ("Node file: '%s'\n", node_file);
  g_print ("PNG file: '%s'\n", png_file);

  window = gdk_surface_new_toplevel (gdk_display_get_default());
  renderer = gsk_renderer_new_for_surface (window);

  /* Load the render node from the given .node file */
  node = load_node_file (node_file);
  if (!node)
    return 1;

  /* Render the .node file and download to cairo surface */
  rendered_texture = gsk_renderer_render_texture (renderer, node, NULL);
  g_assert_nonnull (rendered_texture);

  save_image (rendered_texture, node_file, ".out.png");

  /* Load the given reference png file */
  reference_texture = gdk_texture_new_from_filename (png_file, &error);
  if (reference_texture == NULL)
    {
      g_print ("Error loading reference surface: %s\n", error->message);
      g_clear_error (&error);
      save_image (rendered_texture, node_file, ".out.png");
      return 0;
    }

  /* Now compare the two */
  diff_texture = reftest_compare_textures (rendered_texture, reference_texture);
  if (diff_texture)
    {
      save_image (diff_texture, node_file, ".diff.png");
      g_object_unref (diff_texture);
      success = FALSE;
    }

  g_clear_object (&reference_texture);
  g_clear_object (&rendered_texture);

  if (flip)
    {
      GskRenderNode *node2;
      GdkPixbuf *pixbuf, *pixbuf2;

      node2 = gsk_transform_node_new (node, gsk_transform_scale (NULL, -1, 1));
      rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);

      save_image (rendered_texture, node_file, "-flipped.out.png");

      pixbuf = gdk_pixbuf_new_from_file (png_file, &error);
      pixbuf2 = gdk_pixbuf_flip (pixbuf, TRUE);
      reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
      g_object_unref (pixbuf2);
      g_object_unref (pixbuf);

      save_image (reference_texture, node_file, "-flipped.ref.png");

      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-flipped.diff.png");
          g_object_unref (diff_texture);
          success = FALSE;
        }

      g_clear_object (&rendered_texture);
      g_clear_object (&reference_texture);
      gsk_render_node_unref (node2);
    }

  if (repeat)
    {
      GskRenderNode *node2;
      GdkPixbuf *pixbuf, *pixbuf2;
      int width, height;
      graphene_rect_t node_bounds;
      graphene_rect_t bounds;

      gsk_render_node_get_bounds (node, &node_bounds);
      bounds.origin.x = 0.;
      bounds.origin.y = 0.;
      bounds.size.width = 2 * node_bounds.size.width;
      bounds.size.height = 2 * node_bounds.size.height;

      node2 = gsk_repeat_node_new (&bounds, node, &node_bounds);
      rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);

      save_image (rendered_texture, node_file, "-repeated.out.png");

      pixbuf = gdk_pixbuf_new_from_file (png_file, &error);
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      pixbuf2 = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf),
                                gdk_pixbuf_get_has_alpha (pixbuf),
                                gdk_pixbuf_get_bits_per_sample (pixbuf),
                                width * 2,
                                height * 2);
      gdk_pixbuf_copy_area (pixbuf, 0, 0, width, height, pixbuf2, 0, 0);
      gdk_pixbuf_copy_area (pixbuf, 0, 0, width, height, pixbuf2, width, 0);
      gdk_pixbuf_copy_area (pixbuf, 0, 0, width, height, pixbuf2, 0, height);
      gdk_pixbuf_copy_area (pixbuf, 0, 0, width, height, pixbuf2, width, height);

      reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
      g_object_unref (pixbuf2);
      g_object_unref (pixbuf);

      save_image (reference_texture, node_file, "-repeated.ref.png");

      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-repeated.diff.png");
          g_object_unref (diff_texture);
          success = FALSE;
        }

      g_clear_object (&rendered_texture);
      g_clear_object (&reference_texture);
      gsk_render_node_unref (node2);
    }

  if (rotate)
    {
      GskRenderNode *node2;
      GdkPixbuf *pixbuf, *pixbuf2;

      node2 = gsk_transform_node_new (node, gsk_transform_rotate (NULL, 90));
      rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);

      save_image (rendered_texture, node_file, "-rotated.out.png");

      pixbuf = gdk_pixbuf_new_from_file (png_file, &error);
      pixbuf2 = gdk_pixbuf_rotate_simple (pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
      reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
      g_object_unref (pixbuf2);
      g_object_unref (pixbuf);

      save_image (reference_texture, node_file, "-rotated.ref.png");

      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-rotated.diff.png");
          g_object_unref (diff_texture);
          success = FALSE;
        }

      g_clear_object (&rendered_texture);
      g_clear_object (&reference_texture);
      gsk_render_node_unref (node2);
    }

  gsk_render_node_unref (node);

  return success ? 0 : 1;
}

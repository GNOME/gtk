#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include "reftest-compare.h"

char *
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

  dir = g_get_tmp_dir ();
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

  g_test_message ("Storing test result image at %s", filename);
  g_assert (cairo_surface_write_to_png (surface, filename) == CAIRO_STATUS_SUCCESS);
  g_free (filename);
}

/*
 * Arguments:
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

  g_assert (argc == 3);

  gtk_init ();

  node_file = argv[1];
  png_file = argv[2];

  window = gdk_surface_new_toplevel (gdk_display_get_default(), 10 , 10);
  renderer = gsk_renderer_new_for_window (window);

  g_test_message ("Node file: '%s'\n", node_file);
  g_test_message ("PNG file: '%s'\n", png_file);

  /* Load the render node from the given .node file */
  {
    GBytes *bytes;
    GError *error = NULL;
    gsize len;
    char *contents;

    if (!g_file_get_contents (node_file, &contents, &len, &error))
      {
        g_test_message ("Could not open node file: %s\n", error->message);
        g_clear_error (&error);
        g_test_fail ();
        return -1;
      }

    bytes = g_bytes_new_take (contents, len);
    node = gsk_render_node_deserialize (bytes, &error);
    g_bytes_unref (bytes);

    g_assert (node != NULL);
  }

  /* Render the .node file and download to cairo surface */
  texture = gsk_renderer_render_texture (renderer, node, NULL);
  g_assert (texture != NULL);

  rendered_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                 gdk_texture_get_width (texture),
                                                 gdk_texture_get_height (texture));
  gdk_texture_download (texture,
                        cairo_image_surface_get_data (rendered_surface),
                        cairo_image_surface_get_stride (rendered_surface));
  cairo_surface_mark_dirty (rendered_surface);

  /* Load the given reference png file */
  reference_surface = cairo_image_surface_create_from_png (png_file);
  g_assert (reference_surface != NULL);

  /* Now compare the two */
  diff_surface = reftest_compare_surfaces (rendered_surface, reference_surface);

  save_image (rendered_surface, node_file, ".out.png");

  if (diff_surface)
    save_image (diff_surface, node_file, ".diff.png");

  g_assert (diff_surface == NULL);

  return 0;
}

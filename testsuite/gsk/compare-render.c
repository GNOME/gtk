#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include "../reftests/reftest-compare.h"

static char *arg_dir = NULL;
static char *arg_output_dir = NULL;
static char **arg_skip = NULL;

static GskRenderer *renderer;
static GdkSurface *window;

extern void
replay_node (GskRenderNode *node, GtkSnapshot *snapshot);

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

  if (g_test_verbose ())
    g_test_message ("Storing test result image at %s", filename);
  result = gdk_texture_save_to_png (texture, filename);
  g_assert_true (result);
  g_free (filename);
}

static void
save_node (GskRenderNode *node,
           const char    *test_name,
           const char    *extension)
{
  char *filename = get_output_file (test_name, ".node", extension);
  gboolean result;

  if (g_test_verbose ())
    g_test_message ("Storing modified nodes at %s", filename);
  result = gsk_render_node_write_to_file (node, filename, NULL);
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
      g_test_message ("Could not open node file: %s", error->message);
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

static GdkPixbuf *
apply_mask_to_pixbuf (GdkPixbuf *pixbuf)
{
  GdkPixbuf *copy;

  copy = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
  for (unsigned int j = 0; j < gdk_pixbuf_get_height (copy); j++)
    {
      guint8 *row = gdk_pixbuf_get_pixels (copy) + j * gdk_pixbuf_get_rowstride (copy);
      for (unsigned int i = 0; i < gdk_pixbuf_get_width (copy); i++)
        {
          guint8 *p = row + i * 4;
          if ((i < 25 && j >= 25) || (i >= 25 && j < 25))
            {
              p[0] = p[1] = p[2] = p[3] = 0;
            }
        }
    }

  return copy;
}

enum {
  PLAIN,
  FLIP,
  REPEAT,
  ROTATE,
  MASK,
  REPLAY
};

static const char *kind[] = {
  "plain",
  "flip",
  "repeat",
  "rotate",
  "mask",
  "replay"
};

typedef struct
{
  char *node_file;
  char *png_file;
  int opt;
} TestCase;

static void
test_case_free (gpointer data)
{
  TestCase *c = data;

  g_free (c->node_file);
  g_free (c->png_file);
  g_free (c);
}

static void
test_one_file (gconstpointer data)
{
  const TestCase *c = (TestCase *)data;
  GdkTexture *reference_texture;
  GdkTexture *rendered_texture;
  GdkTexture *diff_texture;
  GskRenderNode *node;
  GError *error = NULL;

  if (g_test_verbose ())
    {
      g_test_message ("Node file: '%s'", c->node_file);
      g_test_message ("PNG file: '%s'", c->png_file);
    }

  /* Load the render node from the given .node file */
  node = load_node_file (c->node_file);
  if (!node)
    {
      g_test_fail_printf ("File %s not found", c->node_file);
      return;
    }

  switch (c->opt)
    {
    case PLAIN:
      {
        /* Render the .node file and download to cairo surface */
        rendered_texture = gsk_renderer_render_texture (renderer, node, NULL);
        g_assert_nonnull (rendered_texture);

        save_image (rendered_texture, c->node_file, ".out.png");

        /* Load the given reference png file */
        reference_texture = gdk_texture_new_from_filename (c->png_file, &error);
        if (reference_texture == NULL)
          {
            save_image (rendered_texture, c->node_file, ".out.png");
            g_test_fail_printf ("Error loading reference surface: %s", error->message);
            g_clear_error (&error);
            return;
          }

        /* Now compare the two */
        diff_texture = reftest_compare_textures (rendered_texture, reference_texture);
        if (diff_texture)
          {
            save_image (diff_texture, c->node_file, ".diff.png");
            g_object_unref (diff_texture);
            g_test_fail ();
          }

        g_clear_object (&reference_texture);
        g_clear_object (&rendered_texture);
      }
      break;

    case FLIP:
      {
        GskRenderNode *node2;
        GdkPixbuf *pixbuf, *pixbuf2;
        GskTransform *transform;

        transform = gsk_transform_scale (NULL, -1, 1);
        node2 = gsk_transform_node_new (node, transform);
        gsk_transform_unref (transform);

        save_node (node2, c->node_file, "-flipped.node");

        rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
        save_image (rendered_texture, c->node_file, "-flipped.out.png");

        pixbuf = gdk_pixbuf_new_from_file (c->png_file, &error);
        g_assert_no_error (error);
        pixbuf2 = gdk_pixbuf_flip (pixbuf, TRUE);
        reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
        g_object_unref (pixbuf2);
        g_object_unref (pixbuf);

        save_image (reference_texture, c->node_file, "-flipped.ref.png");

        diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

        if (diff_texture)
          {
            save_image (diff_texture, c->node_file, "-flipped.diff.png");
            g_object_unref (diff_texture);
            g_test_fail ();
          }

        g_clear_object (&rendered_texture);
        g_clear_object (&reference_texture);
        gsk_render_node_unref (node2);
      }
      break;

    case REPEAT:
      {
        GskRenderNode *node2;
        GdkPixbuf *pixbuf, *pixbuf2, *pixbuf3;
        int width, height;
        graphene_rect_t node_bounds;
        graphene_rect_t bounds;
        float offset_x, offset_y;

        gsk_render_node_get_bounds (node, &node_bounds);

        if (node_bounds.size.width > 32768. || node_bounds.size.height > 32768.)
          {
            g_test_skip_printf ("Avoiding repeat test that exceeds cairo image surface dimensions");
          }
        else
          {
            bounds.origin.x = 0.;
            bounds.origin.y = 0.;
            bounds.size.width = 2 * node_bounds.size.width;
            bounds.size.height = 2 * node_bounds.size.height;

            offset_x = floorf (fmodf (node_bounds.origin.x, node_bounds.size.width));
            offset_y = floorf (fmodf (node_bounds.origin.y, node_bounds.size.height));
            if (offset_x < 0)
              offset_x += node_bounds.size.width;
            if (offset_y < 0)
              offset_y += node_bounds.size.height;

            node2 = gsk_repeat_node_new (&bounds, node, &node_bounds);
            save_node (node2, c->node_file, "-repeated.node");

            rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
            save_image (rendered_texture, c->node_file, "-repeated.out.png");

            pixbuf = gdk_pixbuf_new_from_file (c->png_file, &error);
            g_assert_no_error (error);

            width = gdk_pixbuf_get_width (pixbuf);
            height = gdk_pixbuf_get_height (pixbuf);
            pixbuf2 = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf),
                                      gdk_pixbuf_get_has_alpha (pixbuf),
                                      gdk_pixbuf_get_bits_per_sample (pixbuf),
                                      width * 3,
                                      height * 3);

            for (int i = 0; i < 3; i++)
              for (int j = 0; j < 3; j++)
                gdk_pixbuf_copy_area (pixbuf, 0, 0, width, height, pixbuf2, i * width,  j * height);

            pixbuf3 = gdk_pixbuf_new_subpixbuf (pixbuf2, width - (int) offset_x, height - (int) offset_y, 2 * width, 2 * height);

            reference_texture = gdk_texture_new_for_pixbuf (pixbuf3);

            g_object_unref (pixbuf3);
            g_object_unref (pixbuf2);
            g_object_unref (pixbuf);

            save_image (reference_texture, c->node_file, "-repeated.ref.png");

            diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

            if (diff_texture)
              {
                save_image (diff_texture, c->node_file, "-repeated.diff.png");
                g_object_unref (diff_texture);
                g_test_fail ();
              }

            g_clear_object (&rendered_texture);
            g_clear_object (&reference_texture);
            gsk_render_node_unref (node2);
          }
      }
      break;

    case ROTATE:
      {
        GskRenderNode *node2;
        GdkPixbuf *pixbuf, *pixbuf2;
        GskTransform *transform;

        transform = gsk_transform_rotate (NULL, 90);
        node2 = gsk_transform_node_new (node, transform);
        gsk_transform_unref (transform);

        save_node (node2, c->node_file, "-rotated.node");

        rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
        save_image (rendered_texture, c->node_file, "-rotated.out.png");

        pixbuf = gdk_pixbuf_new_from_file (c->png_file, &error);
        g_assert_no_error (error);
        pixbuf2 = gdk_pixbuf_rotate_simple (pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
        reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
        g_object_unref (pixbuf2);
        g_object_unref (pixbuf);

        save_image (reference_texture, c->node_file, "-rotated.ref.png");

        diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

        if (diff_texture)
          {
            save_image (diff_texture, c->node_file, "-rotated.diff.png");
            g_object_unref (diff_texture);
            g_test_fail ();
          }

        g_clear_object (&rendered_texture);
        g_clear_object (&reference_texture);
        gsk_render_node_unref (node2);
      }
      break;

    case MASK:
      {
        GskRenderNode *node2;
        GdkPixbuf *pixbuf, *pixbuf2;
        graphene_rect_t bounds;
        GskRenderNode *mask_node;
        GskRenderNode *nodes[2];

        gsk_render_node_get_bounds (node, &bounds);
        nodes[0] = gsk_color_node_new (&(GdkRGBA){ 0, 0, 0, 1},
                                       &GRAPHENE_RECT_INIT (bounds.origin.x, bounds.origin.y, 25, 25));
        if (bounds.size.width > 25 && bounds.size.height > 25)
          {
            nodes[1] = gsk_color_node_new (&(GdkRGBA){ 0, 0, 0, 1},
                                           &GRAPHENE_RECT_INIT (bounds.origin.x + 25, bounds.origin.y + 25, bounds.size.width - 25, bounds.size.height - 25));

            mask_node = gsk_container_node_new (nodes, G_N_ELEMENTS (nodes));
            gsk_render_node_unref (nodes[0]);
            gsk_render_node_unref (nodes[1]);
          }
        else
          {
            mask_node = nodes[0];
          }

        node2 = gsk_mask_node_new (node, mask_node, GSK_MASK_MODE_ALPHA);
        gsk_render_node_unref (mask_node);
        save_node (node2, c->node_file, "-masked.node");

        rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
        save_image (rendered_texture, c->node_file, "-masked.out.png");

        pixbuf = gdk_pixbuf_new_from_file (c->png_file, &error);
        g_assert_no_error (error);
        pixbuf2 = apply_mask_to_pixbuf (pixbuf);
        reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
        g_object_unref (pixbuf2);
        g_object_unref (pixbuf);

        save_image (reference_texture, c->node_file, "-masked.ref.png");

        diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

        if (diff_texture)
          {
            save_image (diff_texture, c->node_file, "-masked.diff.png");
            g_object_unref (diff_texture);
            g_test_fail ();
          }

        g_clear_object (&rendered_texture);
        g_clear_object (&reference_texture);
        gsk_render_node_unref (node2);
      }
      break;

    case REPLAY:
      {
        GskRenderNode *node2;
        GdkTexture *rendered_texture2;
        graphene_rect_t node_bounds, node2_bounds;
        GtkSnapshot *snapshot = gtk_snapshot_new ();

        replay_node (node, snapshot);
        node2 = gtk_snapshot_free_to_node (snapshot);
        /* If the whole render node tree got eliminated, make sure we have
           something to work with nevertheless.  */
        if (!node2)
          node2 = gsk_container_node_new (NULL, 0);

        gsk_render_node_get_bounds (node, &node_bounds);
        gsk_render_node_get_bounds (node2, &node2_bounds);
        /* Check that the node didn't grow.  */
        if (!graphene_rect_contains_rect (&node_bounds, &node2_bounds))
          g_test_fail_printf ("The node grew");

        rendered_texture = gsk_renderer_render_texture (renderer, node, &node_bounds);
        rendered_texture2 = gsk_renderer_render_texture (renderer, node2, &node_bounds);
        g_assert_nonnull (rendered_texture);
        g_assert_nonnull (rendered_texture2);

        diff_texture = reftest_compare_textures (rendered_texture, rendered_texture2);

        if (diff_texture)
          {
            save_image (diff_texture, c->node_file, "-replayed.diff.png");
            save_node (node2, c->node_file, "-replayed.node");
            g_object_unref (diff_texture);
            g_test_fail ();
          }

        g_clear_object (&rendered_texture);
        g_clear_object (&rendered_texture2);
        gsk_render_node_unref (node2);
      }
      break;

    default:
      g_assert_not_reached ();
    }

  gsk_render_node_unref (node);
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  const char *path1, *path2;

  path1 = g_file_peek_path (file1);
  path2 = g_file_peek_path (file2);

  return strcmp (path1, path2);
}

static gboolean
should_skip (const char *filename)
{
  if (arg_skip)
    for (int i = 0; arg_skip[i]; i++)
      {
        if (strstr (filename, arg_skip[i]))
          return TRUE;
      }

  return FALSE;
}

static void
add_files_in_directory (GFile *dir)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *l, *files;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".node"))
        {
          g_object_unref (info);
          continue;
        }

      if (should_skip (filename))
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
  for (l = files; l; l = l->next)
    {
      GFile *file = l->data;
      char *name;

      name = g_file_get_basename (file);

      for (int opt = PLAIN; opt <= REPLAY; opt++)
        {
          TestCase *c;
          char *path;

          c = g_new0 (TestCase, 1);

          c->node_file = g_file_get_path (file);
          c->png_file = file_replace_extension (c->node_file, ".node", ".png");
          c->opt = opt;

          path = g_strconcat ("/gsk/compare/", kind[opt], "/", name, NULL);
          g_test_add_data_func_full (path, c, test_one_file, test_case_free);
          g_free (path);
        }

      g_free (name);
    }

  g_list_free_full (files, g_object_unref);
}

static gboolean flip;
static gboolean rotate;
static gboolean repeat;
static gboolean mask;
static gboolean replay;

static const GOptionEntry options[] = {
  { "dir", 0, 0, G_OPTION_ARG_FILENAME, &arg_dir, "Directory with test files", "DIR" },
  { "output", 0, 0, G_OPTION_ARG_FILENAME, &arg_output_dir, "Directory to save image files to", "DIR" },
  { "skip", 0, 0, G_OPTION_ARG_STRING_ARRAY, &arg_skip, "Files to skip" },
  { "flip", 0, 0, G_OPTION_ARG_NONE, &flip, "Do flipped test", NULL },
  { "rotate", 0, 0, G_OPTION_ARG_NONE, &rotate, "Do rotated test", NULL },
  { "repeat", 0, 0, G_OPTION_ARG_NONE, &repeat, "Do repeated test", NULL },
  { "mask", 0, 0, G_OPTION_ARG_NONE, &mask, "Do masked test", NULL },
  { "replay", 0, 0, G_OPTION_ARG_NONE, &replay, "Do replay test", NULL },
  { NULL }
};

int
main (int argc, char *argv[])
{
  GOptionContext *context;
  GError *error = NULL;
  GFile *file;
  int retval;

  gtk_init ();
  (g_test_init) (&argc, &argv, NULL);

  context = g_option_context_new ("- run GSK compare tests");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return 1;
    }
  else if (argc == 3)
    {
      TestCase *c;
      char *name;
      char *path;

      c = g_new (TestCase, 1);
      c->node_file = g_strdup (argv[1]);
      c->png_file = g_strdup (argv[2]);
      if (flip)
        c->opt = FLIP;
      else if (rotate)
        c->opt = ROTATE;
      else if (repeat)
        c->opt = REPEAT;
      else if (mask)
        c->opt = MASK;
      else if (replay)
        c->opt = REPLAY;
      else
        c->opt = PLAIN;

      file = g_file_new_for_commandline_arg (argv[1]);
      name = g_file_get_basename (file);
      path = g_strconcat ("/gsk/compare/", kind[c->opt], "/", name, NULL);
      g_test_add_data_func_full (path, c, test_one_file, test_case_free);
      g_free (path);
      g_free (name);
      g_object_unref (file);
    }

  g_option_context_free (context);

  if (arg_dir)
    {
      file = g_file_new_for_commandline_arg (arg_dir);
      add_files_in_directory (file);
      g_object_unref (file);
    }

  window = gdk_surface_new_toplevel (gdk_display_get_default());
  renderer = gsk_renderer_new_for_surface (window);

  retval = g_test_run ();

  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
  gdk_surface_destroy (window);

  return retval;
}

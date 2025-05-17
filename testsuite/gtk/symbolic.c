#include <gtk/gtk.h>
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
                 const char *variant,
                 const char *orig_ext,
                 const char *new_ext)
{
  const char *dir;
  char *result, *base;
  char *name;

  dir = get_output_dir ();
  base = g_path_get_basename (file);
  if (variant)
    {
      char *s = file_replace_extension (base, orig_ext, "");
      name = g_strconcat (s, "-", variant, new_ext, NULL);
      g_free (s);
    }
  else
    {
      name = file_replace_extension (base, orig_ext, new_ext);
    }

  result = g_strconcat (dir, G_DIR_SEPARATOR_S, name, NULL);

  g_free (base);
  g_free (name);

  return result;
}

static void
save_image (GdkTexture *texture,
            const char *file,
            const char *variant_name,
            const char *extension)
{
  char *filename = get_output_file (file, variant_name, ".svg", extension);
  gboolean result;

  g_print ("Storing test result image at %s\n", filename);
  result = gdk_texture_save_to_png (texture, filename);
  g_assert_true (result);
  g_free (filename);
}

static GdkTexture *
svg_to_texture (const char *filename)
{
  GFile *file;
  GtkIconPaintable *icon;
  GtkSnapshot *snapshot;
  GdkRGBA colors[4] = {
    { 0, 0, 0, 1 },
    { 1, 0, 0, 1 },
    { 1, 1, 0, 1 },
    { 0, 1, 0, 1 },
  };
  GskRenderNode *node;
  GdkSurface *surface;
  GskRenderer *renderer;
  GdkTexture *texture;

  file = g_file_new_for_commandline_arg (filename);
  icon = gtk_icon_paintable_new_for_file (file, 16, 1);

  snapshot = gtk_snapshot_new ();
  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (icon),
                                            snapshot,
                                            16, 16,
                                            colors,
                                            G_N_ELEMENTS (colors));
  node = gtk_snapshot_free_to_node (snapshot);

  surface = gdk_surface_new_toplevel (gdk_display_get_default ());
  renderer = gsk_renderer_new_for_surface (surface);

  texture = gsk_renderer_render_texture (renderer, node, &GRAPHENE_RECT_INIT (0, 0, 16, 16));

  gsk_render_node_unref (node);

  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
  gdk_surface_destroy (surface);

  g_object_unref (icon);
  g_object_unref (file);

  return texture;
}

static void
test_symbolic (gconstpointer data)
{
  GFile *file = (GFile *) data;
  const char *filename;
  char *ref_filename;
  GdkTexture *texture, *reference, *diff;
  GError *error = NULL;

  filename = g_file_peek_path (file);
  texture = svg_to_texture (filename);

  ref_filename = file_replace_extension (filename, ".svg", ".png");
  reference = gdk_texture_new_from_filename (ref_filename, &error);
  if (reference == NULL)
    {
      g_print ("Error loading reference image: %s\n", error->message);
      g_clear_error (&error);
      g_test_fail ();
      return;
    }

  diff = reftest_compare_textures (reference, texture);
  if (diff)
    {
      save_image (texture, filename, NULL, ".out.png");
      save_image (diff, filename, NULL, ".diff.png");
      g_test_fail ();
    }

  g_object_unref (reference);
  g_free (ref_filename);
  g_object_unref (texture);
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

int
main (int argc, char *argv[])
{
  GOptionEntry options[] = {
    { "output", 0, 0, G_OPTION_ARG_FILENAME, &arg_output_dir, "Directory to save image files to", "DIR" },
    { NULL }
  };
  GOptionContext *context;
  char *dir;
  GFile *file;
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;

  gtk_test_init (&argc, &argv);

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  g_option_context_free (context);

  if (argc > 1)
    {
      GdkTexture *texture;
      char *out;

      texture = svg_to_texture (argv[1]);
      g_assert_nonnull (texture);

      out = file_replace_extension (argv[1], ".svg", ".png");
      gdk_texture_save_to_png (texture, out);

      g_free (out);
      g_object_unref (texture);

      return 0;
    }

  dir = g_test_build_filename (G_TEST_DIST, "symbolic-icons", NULL);
  file = g_file_new_for_path (dir);

  enumerator = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL)
    {
      const char *filename;

      filename = g_file_info_get_name (info);
      if (!g_str_has_suffix (filename, ".svg"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (file, filename));

      g_object_unref (info);
    }
  g_object_unref (enumerator);
  g_object_unref (file);
  g_free (dir);

  files = g_list_sort (files, compare_files);
  for (GList *l = files; l; l = l->next)
    {
      file = l->data;
      char *basename, *testname;

      basename = g_file_get_basename (file);
      testname = g_strconcat ("/symbolic/", basename, NULL);
      g_test_add_data_func_full (testname, g_object_ref (file), test_symbolic, g_object_unref);
      g_free (testname);
      g_free (basename);
    }
  g_list_free_full (files, g_object_unref);

  return g_test_run ();
}

#include <gtk/gtk.h>
#include "gtkiconthemeprivate.h"
#include "gdkrgbaprivate.h"
#include "../reftests/reftest-compare.h"

static GskRenderer *
get_renderer (void)
{
  static GskRenderer *renderer;

  if (!renderer)
    {
      GdkDisplay *display;
      GdkSurface *surface;
      GError *error = NULL;

      display = gdk_display_get_default ();
      surface = gdk_surface_new_toplevel (display);

      renderer = gsk_renderer_new_for_surface (surface);

      g_assert_no_error (error);
    }

  return renderer;
}

static const char *
get_output_dir (void)
{
  static const char *output_dir = NULL;
  GError *error = NULL;
  GFile *file;

  if (output_dir)
    return output_dir;

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
            const char *test_name,
            const char *variant_name,
            const char *extension)
{
  char *filename = get_output_file (test_name, variant_name, ".svg", extension);
  gboolean result;

  g_print ("Storing test result image at %s\n", filename);
  result = gdk_texture_save_to_png (texture, filename);
  g_assert_true (result);
  g_free (filename);
}

static void
save_node (GskRenderNode *node,
           const char    *test_name,
           const char    *variant_name,
           const char    *extension)
{
  char *filename = get_output_file (test_name, variant_name, ".svg", extension);
  gboolean result;

  g_print ("Storing test result node at %s\n", filename);
  result = gsk_render_node_write_to_file (node, filename, NULL);
  g_assert_true (result);
  g_free (filename);
}

static void
compare_nodes (GskRenderNode *node1,
               GskRenderNode *node2,
               const char    *path,
               const char    *variant)
{
  GdkTexture *texture1, *texture2, *diff;

  texture1 = gsk_renderer_render_texture (get_renderer (), node1, NULL);
  texture2 = gsk_renderer_render_texture (get_renderer (), node2, NULL);

  diff = reftest_compare_textures (texture1, texture2);
  if (diff)
    {
      save_node (node1, path, variant, "-1.node");
      save_node (node2, path, variant, "-2.node");
      save_image (diff, path, variant, ".diff.png");
      g_object_unref (diff);
      g_test_fail ();
    }

  g_object_unref (texture1);
  g_object_unref (texture2);
}

static GskRenderNode *
snapshot_symbolic (GtkIconPaintable *icon)
{
  const GdkRGBA colors[4] = {
    GDK_RGBA("000000"),
    GDK_RGBA("ff0000"),
    GDK_RGBA("daa520"),
    GDK_RGBA("ff69b4"),
  };
  GtkSnapshot *snapshot;

  snapshot = gtk_snapshot_new ();
  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (icon),
                                            snapshot,
                                            64, 64,
                                            colors, sizeof (colors));
  return gtk_snapshot_free_to_node (snapshot);
}

static void
compare_symbolic (GtkIconPaintable *icon1,
                  GtkIconPaintable *icon2,
                  const char       *path,
                  const char       *variant)
{
  GskRenderNode *node1, *node2;

  node1 = snapshot_symbolic (icon1);
  node2 = snapshot_symbolic (icon2);

  compare_nodes (node1, node2, path, variant);
}

static void
test_symbolic (gconstpointer data)
{
  const char *path = data;
  char *uri;
  GFile *file;
  GtkIconPaintable *icon1, *icon2;

  uri = g_strconcat ("resource:", path, NULL);
  file = g_file_new_for_uri (uri);

  icon1 = gtk_icon_paintable_new_for_file (file, 64, 1);
  gtk_icon_paintable_set_debug (icon1, FALSE, FALSE, FALSE);

  for (int allow_node = 0; allow_node <= 1; allow_node++)
    for (int allow_recolor = 0; allow_recolor <= 1; allow_recolor++)
      for (int allow_mask = 0; allow_mask <= 1; allow_mask++)
        {
          char variant[16];
          g_snprintf (variant, sizeof (variant), "%d%d%d", allow_node, allow_recolor, allow_mask);
          icon2 = gtk_icon_paintable_new_for_file (file, 64, 1);
          gtk_icon_paintable_set_debug (icon2, allow_node, allow_recolor, allow_mask);
          compare_symbolic (icon1, icon2, path, variant);

          g_object_unref (icon2);
        }

  g_object_unref (icon1);

  g_object_unref (file);
  g_free (uri);
}

int
main (int argc, char *argv[])
{
  const char *path = "/org/gtk/libgtk/icons/scalable";
  char **dirs;

  gtk_test_init (&argc, &argv);

  dirs = g_resources_enumerate_children (path, 0, NULL);
  for (int i = 0; dirs[i]; i++)
    {
      char *dir = g_strconcat (path, "/", dirs[i], NULL);
      char **names;

      names = g_resources_enumerate_children (dir, 0, NULL);
      for (int j = 0; names[j]; j++)
        {
          char *testname = g_strconcat ("/symbolic/", names[j], NULL);
          g_test_add_data_func_full (testname, g_strconcat (dir, "/", names[j], NULL), test_symbolic, g_free);
          g_free (testname);
        }
      g_strfreev (names);
    }
  g_strfreev (dirs);

  return g_test_run ();
}

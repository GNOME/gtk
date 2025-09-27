#include <gtk/gtk.h>
#include "gtkiconthemeprivate.h"
#include "gtkiconpaintableprivate.h"
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
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 64, 64);

  texture1 = gsk_renderer_render_texture (get_renderer (), node1, &bounds);
  texture2 = gsk_renderer_render_texture (get_renderer (), node2, &bounds);

  diff = reftest_compare_textures (texture1, texture2);
  if (diff)
    {
      g_print ("Test failed for %s (%s)\n", path, variant);
      save_node (node1, path, variant, "-1.node");
      save_node (node2, path, variant, "-2.node");
      save_image (diff, path, variant, ".diff.png");
      save_image (texture1, path, variant, "-1.png");
      save_image (texture2, path, variant, "-2.png");
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

static gboolean
pixbuf_supports_svg (void)
{
  GSList *formats, *l;
  gboolean has_svg = FALSE;

  formats = gdk_pixbuf_get_formats ();
  for (l = formats; l; l = l->next)
    {
      GdkPixbufFormat *format = l->data;
      char *name;

      name = gdk_pixbuf_format_get_name (format);
      has_svg = strcmp (name, "svg") == 0;
      g_free (name);

      if (has_svg)
        break;
    }
  g_slist_free (formats);

  return has_svg;
}

static void
test_symbolic_file (gconstpointer data)
{
  GFile *file = G_FILE (data);
  GtkIconPaintable *icon1, *icon2;
  char *uri;
  const char *scheme;
  const char *path;

  if (!pixbuf_supports_svg ())
    {
      g_test_skip ("No support for loading svgs as texture");
      return;
    }

  uri = g_file_get_uri (file);
  scheme = g_uri_peek_scheme (uri);
  path = uri + strlen (scheme) + strlen ("://");

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
  g_free (uri);
}

static const char *skipped[] = {
  "/org/gtk/libgtk/icons/emoji-objects-symbolic.svg",
  "/org/gtk/libgtk/icons/folder-publicshare-symbolic.svg",
  NULL
};

static void
test_symbolic_resource (gconstpointer data)
{
  const char *path = data;
  char *uri;
  GFile *file;

  if (g_strv_contains (skipped, path))
    {
      g_test_skip ("hard to overcome 1-bit differences");
      return;
    }

  uri = g_strconcat ("resource:", path, NULL);
  file = g_file_new_for_uri (uri);

  test_symbolic_file (file);

  g_object_unref (file);
  g_free (uri);
}

int
main (int argc, char *argv[])
{
  const char *path = "/org/gtk/libgtk/icons";
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

          g_test_add_data_func_full (testname, g_strconcat (dir, names[j], NULL), test_symbolic_resource, g_free);
          g_free (testname);
        }
      g_strfreev (names);
    }
  g_strfreev (dirs);

  if (argc > 1 && strcmp (argv[1], "--include-theme") == 0)
    {
      GdkDisplay *display;
      GtkIconTheme *icon_theme;
      char **names;

      display = gdk_display_get_default ();
      icon_theme = gtk_icon_theme_get_for_display (display);
      names = gtk_icon_theme_get_icon_names (icon_theme);

      for (int i = 0; names[i]; i++)
        {
          GtkIconPaintable *icon;
          GFile *file;
          char *testname;

          icon = gtk_icon_theme_lookup_icon (icon_theme, names[i], NULL, 64, 1, GTK_TEXT_DIR_LTR, GTK_ICON_LOOKUP_FORCE_SYMBOLIC);

          if (!icon)
            continue;

          file = gtk_icon_paintable_get_file (icon);

          testname = g_strconcat ("/theme/", names[i], NULL);
          g_test_add_data_func_full (testname, file, test_symbolic_file, g_object_unref);
          g_free (testname);
        }
    }

  return g_test_run ();
}

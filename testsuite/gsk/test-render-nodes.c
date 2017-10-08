/*
 * Copyright (C) 2017 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
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
#include "reftest-compare.h"

static void
hsv_to_rgb (GdkRGBA *rgba,
            gdouble  h,
            gdouble  s,
            gdouble  v)
{
  gdouble hue, saturation, value;
  gdouble f, p, q, t;

  rgba->alpha = 1.0;

  if ( s == 0.0)
    {
      rgba->red = v;
      rgba->green = v;
      rgba->blue = v; /* heh */
    }
  else
    {
      hue = h * 6.0;
      saturation = s;
      value = v;

      if (hue == 6.0)
        hue = 0.0;

      f = hue - (int) hue;
      p = value * (1.0 - saturation);
      q = value * (1.0 - saturation * f);
      t = value * (1.0 - saturation * (1.0 - f));

      switch ((int) hue)
        {
        case 0:
          rgba->red = value;
          rgba->green = t;
          rgba->blue = p;
          break;

        case 1:
          rgba->red = q;
          rgba->green = value;
          rgba->blue = p;
          break;

        case 2:
          rgba->red = p;
          rgba->green = value;
          rgba->blue = t;
          break;

        case 3:
          rgba->red = p;
          rgba->green = q;
          rgba->blue = value;
          break;

        case 4:
          rgba->red = t;
          rgba->green = p;
          rgba->blue = value;
          break;

        case 5:
          rgba->red = value;
          rgba->green = p;
          rgba->blue = q;
          break;

        default:
          g_assert_not_reached ();
        }
    }
}

static GskRenderNode *
colors (void)
{
  GskRenderNode **nodes = g_newa (GskRenderNode *, 1000);
  GskRenderNode *container;
  graphene_rect_t bounds;
  GdkRGBA color;
  guint i;

  for (i = 0; i < 1000; i++)
    {
      bounds.size.width = g_random_int_range (20, 100);
      bounds.origin.x = g_random_int_range (0, 1000 - bounds.size.width);
      bounds.size.height = g_random_int_range (20, 100);
      bounds.origin.y = g_random_int_range (0, 1000 - bounds.size.height);
      hsv_to_rgb (&color, g_random_double (), g_random_double_range (0.15, 0.4), g_random_double_range (0.6, 0.85));
      color.alpha = g_random_double_range (0.5, 0.75);
      nodes[i] = gsk_color_node_new (&color, &bounds);
    }

  container = gsk_container_node_new (nodes, 1000);

  for (i = 0; i < 1000; i++)
    gsk_render_node_unref (nodes[i]);

  return container;
}

static GskRenderNode *
cairo (void)
{
  GskRenderNode *node;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 200, 600);
  cr = cairo_create (surface);

  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_rectangle (cr, 0, 0, 200, 200);
  cairo_fill (cr);
  cairo_set_source_rgb (cr, 0, 1, 0);
  cairo_rectangle (cr, 0, 200, 200, 200);
  cairo_fill (cr);
  cairo_set_source_rgb (cr, 0, 0, 1);
  cairo_rectangle (cr, 0, 400, 200, 200);
  cairo_fill (cr);

  node = gsk_cairo_node_new_for_surface (&GRAPHENE_RECT_INIT (0, 0, 200, 600), surface);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return node;
}

static const struct {
  const char *name;
  GskRenderNode * (* func) (void);
} functions[] = {
  { "colors.node", colors },
  { "cairo.node", cairo },
};

/*** test setup ***/

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

static void
save_image (cairo_surface_t *surface,
            const char      *test_name,
            const char      *extension)
{
  char *filename = file_replace_extension (test_name, ".node", extension);

  g_test_message ("Storing test result image at %s", filename);
  g_assert (cairo_surface_write_to_png (surface, filename) == CAIRO_STATUS_SUCCESS);
  g_free (filename);
}

static void
load_node_file (GFile *file, gboolean generate)
{
  char *node_file;
  GError *error = NULL;
  char *contents;
  gsize len;
  GBytes *bytes;
  GskRenderNode *node;
  GskRenderer *renderer;
  GdkWindow *window;
  GskTexture *texture = NULL;
  cairo_surface_t *surface;
  char *png_file;
  cairo_surface_t *ref_surface;
  cairo_surface_t *diff_surface;
  const char *ext;

  node_file = g_file_get_path (file);

  if (!g_file_get_contents (node_file, &contents, &len, &error))
    {
      g_test_message ("Could not open node file: %s\n", error->message);
      g_clear_error (&error);
      g_test_fail ();
      return;
    }
  bytes = g_bytes_new_take (contents, len);
  node = gsk_render_node_deserialize (bytes, &error);
  g_bytes_unref (bytes);

  if (node == NULL)
    {
      g_test_message ("Invalid node file: %s\n", error->message);
      g_clear_error (&error);
      g_test_fail ();
      return;
    }

  window = gdk_window_new_toplevel (gdk_display_get_default(), 0, 10 , 10);
  renderer = gsk_renderer_new_for_window (window);
  texture = gsk_renderer_render_texture (renderer, node, NULL);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        gsk_texture_get_width (texture),
                                        gsk_texture_get_height (texture));
  gsk_texture_download (texture,
                        cairo_image_surface_get_data (surface),
                        cairo_image_surface_get_stride (surface));
  cairo_surface_mark_dirty (surface);

  if (strcmp (G_OBJECT_TYPE_NAME (renderer), "GskVulkanRenderer") == 0)
    ext = ".vulkan.png";
  else if (strcmp (G_OBJECT_TYPE_NAME (renderer), "GskGLRenderer") == 0)
    ext = ".gl.png";
  else if (strcmp (G_OBJECT_TYPE_NAME (renderer), "GskCairoRenderer") == 0)
    ext = ".cairo.png";
  else
    ext = ".png";

  g_object_unref (texture);
  g_object_unref (window);
  g_object_unref (renderer);
  gdk_window_destroy (window);

  gsk_render_node_unref (node);

  if (generate)
    {
      cairo_status_t status;
      char *out_file;

      out_file = file_replace_extension (node_file, ".node", ".png");

      status = cairo_surface_write_to_png (surface, out_file);
      cairo_surface_destroy (surface);
      if (status != CAIRO_STATUS_SUCCESS)
        {
          g_print ("Failed to save png file %s: %s\n", out_file, cairo_status_to_string (status));
          exit (1);
        }
      g_free (out_file);
      return;
    }

  png_file = file_replace_extension (node_file, ".node", ext);
  if (!g_file_test (png_file, G_FILE_TEST_EXISTS))
    {
      g_free (png_file);
      png_file = file_replace_extension (node_file, ".node", ".png");
    }

  ref_surface = cairo_image_surface_create_from_png (png_file);
  diff_surface = reftest_compare_surfaces (surface, ref_surface);

  save_image (surface, node_file, ".out.png");
  save_image (ref_surface, node_file, ".ref.png");
  if (diff_surface)
    {
      save_image (diff_surface, node_file, ".diff.png");
      g_test_fail ();
    }

  cairo_surface_destroy (surface);
  cairo_surface_destroy (ref_surface);
  if (diff_surface)
    cairo_surface_destroy (diff_surface);

  g_free (png_file);
  g_free (node_file);
}

static void
test_node_file (GFile *file)
{
  load_node_file (file, FALSE);
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
                     (GTestFixtureFunc) test_node_file,
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

      if (!g_str_has_suffix (filename, ".node") ||
          g_str_has_suffix (filename, ".png"))
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

static void
generate (const char *name)
{
  int i;
  GError *error = NULL;

  for (i = 0; i < G_N_ELEMENTS (functions); i++)
    {
      if (strcmp (name, functions[i].name) == 0)
        {
          GskRenderNode *node = functions[i].func ();
          if (!gsk_render_node_write_to_file (node, name, &error))
            {
              g_print ("Error writing '%s': %s\n", name, error->message);
              g_clear_error (&error);
              exit (1);
            }
          else
            {
              GFile *file = g_file_new_for_commandline_arg (name);
              load_node_file (file, TRUE);
              g_object_unref (file);
            }

          gsk_render_node_unref (node);
          break;
        }
    }
}

int
main (int argc, char **argv)
{
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
  else if (strcmp (argv[1], "--generate") == 0)
    {
      if (argc >= 3)
        generate (argv[2]);
    }
  else
    {
      guint i;

      for (i = 1; i < argc; i++)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[i]);
          add_test_for_file (file);
          g_object_unref (file);
        }
    }

  return g_test_run ();
}

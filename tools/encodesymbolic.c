/* encodesymbolic.c
 * Copyright (C) 2014  Alexander Larsson <alexl@redhat.com>
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

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <locale.h>

static GdkTexture *
svg_to_texture (GtkSvg  *svg,
                int      width,
                int      height,
                GdkRGBA *colors,
                size_t   n_colors)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  cairo_surface_t *surface;
  cairo_t *cr;
  GBytes *bytes;
  GdkTexture *texture;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  snapshot = gtk_snapshot_new ();

  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (svg),
                                            snapshot,
                                            width, height,
                                            colors, n_colors);

  node = gtk_snapshot_free_to_node (snapshot);

  if (node)
    {
      cr = cairo_create (surface);
      gsk_render_node_draw (node, cr);
      cairo_destroy (cr);

      gsk_render_node_unref (node);
    }

  bytes = g_bytes_new (cairo_image_surface_get_data (surface),
                       height * cairo_image_surface_get_stride (surface));
  texture = gdk_memory_texture_new (width, height, GDK_MEMORY_DEFAULT,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));
  g_bytes_unref (bytes);
  cairo_surface_destroy (surface);

  return texture;
}

static GdkTexture *
load_symbolic_svg (GtkSvg      *svg,
                   int          width,
                   int          height,
                   const char  *fg_color,
                   const char  *success_color,
                   const char  *warning_color,
                   const char  *error_color,
                   GError     **error)
{
  GdkRGBA colors[4];

  gdk_rgba_parse (&colors[GTK_SYMBOLIC_COLOR_FOREGROUND], fg_color);
  gdk_rgba_parse (&colors[GTK_SYMBOLIC_COLOR_SUCCESS], success_color);
  gdk_rgba_parse (&colors[GTK_SYMBOLIC_COLOR_WARNING], warning_color);
  gdk_rgba_parse (&colors[GTK_SYMBOLIC_COLOR_ERROR], error_color);

  return svg_to_texture (svg, width, height, colors, 4);
}

static gboolean
extract_plane (GdkTexture *src,
               guchar     *dst_data,
               gsize       dst_width,
               gsize       dst_height,
               int         from_plane,
               int         to_plane)
{
  const guchar *src_data, *src_row;
  int width, height;
  gsize src_stride, dst_stride;
  guchar *dst_row;
  int x, y;
  gboolean all_clear = TRUE;
  GdkTextureDownloader *downloader;
  GBytes *bytes;

  width = gdk_texture_get_width (src);
  height = gdk_texture_get_height (src);

  g_assert (width <= dst_width);
  g_assert (height <= dst_height);

  downloader = gdk_texture_downloader_new (src);
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R8G8B8A8);
  bytes = gdk_texture_downloader_download_bytes (downloader, &src_stride);

  src_data = g_bytes_get_data (bytes, NULL);

  dst_stride = dst_width * 4;

  for (y = 0; y < height; y++)
    {
      src_row = src_data + src_stride * y;
      dst_row = dst_data + dst_stride * y;
      for (x = 0; x < width; x++)
        {
          if (src_row[from_plane] != 0)
            all_clear = FALSE;

          dst_row[to_plane] = src_row[from_plane];
          src_row += 4;
          dst_row += 4;
        }
    }

  gdk_texture_downloader_free (downloader);
  g_bytes_unref (bytes);

  return all_clear;
}

static GdkTexture *
keep_alpha (GdkTexture *src)
{
  GdkTextureDownloader *downloader;
  GBytes *bytes;
  gsize stride;
  gsize width, height;
  guchar *data;
  gsize size;
  GdkTexture *res;

  width = gdk_texture_get_width (src);
  height = gdk_texture_get_height (src);

  downloader = gdk_texture_downloader_new (src);
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R8G8B8A8);
  data = g_bytes_unref_to_data (gdk_texture_downloader_download_bytes (downloader, &stride), &size);

  for (gsize y = 0; y < height; y++)
    {
      guchar *row = data + stride * y;
      for (gsize x = 0; x < width; x++)
        {
          row[0] = row[1] = row[2] = 0;
          row += 4;
        }
    }

  bytes = g_bytes_new_take (data, size);
  res = gdk_memory_texture_new (width, height, GDK_MEMORY_R8G8B8A8, bytes, stride);
  g_bytes_unref (bytes);
  g_object_unref (src);

  gdk_texture_downloader_free (downloader);

  return res;
}

static gboolean
svg_has_symbolic_classes (GBytes *bytes)
{
#ifdef HAVE_MEMMEM
  const char *data;
  gsize len;

  data = g_bytes_get_data (bytes, &len);

  /* Not super precise, but good enough */
  return memmem (data, len, "class=\"", strlen ("class=\"")) != NULL;
#else
  return TRUE;
#endif
}

static GdkTexture *
gdk_texture_new_from_bytes_symbolic (GBytes    *bytes,
                                     int        width,
                                     int        height,
                                     GError   **error)

{
  GtkSvg *svg;
  const char *r_string = "rgb(255,0,0)";
  const char *g_string = "rgb(0,255,0)";
  guchar *data;
  GBytes *data_bytes;
  GdkTexture *texture;

  svg = gtk_svg_new_from_bytes (bytes);

  if (width == 0 && height == 0)
    {
      width = gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (svg));
      height = gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (svg));
      if (width == 0 || height == 0)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Symbolic icon has no intrinsic size; please set one in its SVG");
          g_object_unref (svg);
          return NULL;
        }
    }

  if (!svg_has_symbolic_classes (bytes))
    {
      texture = svg_to_texture (svg, width, height, NULL, 0);

      if (texture)
        texture = keep_alpha (texture);

      g_object_unref (svg);

      return texture;
    }

  texture = NULL;

  data = NULL;
  data_bytes = NULL;

  for (int plane = 0; plane < 3; plane++)
    {
      GdkTexture *loaded;

      /* Here we render the svg with all colors solid, this should
       * always make the alpha channel the same and it should match
       * the final alpha channel for all possible renderings. We
       * Just use it as-is for final alpha.
       *
       * For the 3 non-fg colors, we render once each with that
       * color as red, and every other color as green. The resulting
       * red will describe the amount of that color is in the
       * opaque part of the color. We store these as the rgb
       * channels, with the color of the fg being implicitly
       * the "rest", as all color fractions should add up to 1.
       */
      loaded = load_symbolic_svg (svg, width, height,
                                  g_string,
                                  plane == 0 ? r_string : g_string,
                                  plane == 1 ? r_string : g_string,
                                  plane == 2 ? r_string : g_string,
                                  error);
      if (loaded == NULL)
        goto out;

      if (plane == 0)
        {
          data = g_new0 (guchar, 4 * width * height);
          data_bytes = g_bytes_new_take (data, 4 * width * height);

          extract_plane (loaded, data, width, height, 3, 3);
        }

      extract_plane (loaded, data, width, height, 0, plane);

      g_object_unref (loaded);
    }

  texture = gdk_memory_texture_new (width, height,
                                    GDK_MEMORY_R8G8B8A8,
                                    data_bytes,
                                    4 * width);

out:
  g_bytes_unref (data_bytes);
  g_object_unref (svg);

  return texture;
}

static GdkTexture *
gdk_texture_new_from_filename_symbolic (const char  *filename,
                                        int          width,
                                        int          height,
                                        GError     **error)
{
  GFile *file;
  GBytes *bytes;
  GdkTexture *texture;

  file = g_file_new_for_path (filename);
  bytes = g_file_load_bytes (file, NULL, NULL, error);
  g_object_unref (file);
  if (!bytes)
    return NULL;

  texture = gdk_texture_new_from_bytes_symbolic (bytes, width, height, error);
  g_bytes_unref (bytes);

  return texture;
}

static char *output_dir = NULL;

static gboolean debug;

static GOptionEntry args[] = {
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_dir, N_("Output to this directory instead of cwd"), NULL },
  { "debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Generate debug output") },
  { NULL }
};

int
main (int argc, char **argv)
{
  char *path, *basename, *pngpath, *pngfile, *dot;
  GOptionContext *context;
  GdkTexture *symbolic;
  GError *error;
  int width, height;
  char **sizev;
  GFile *dest;
  GBytes *bytes;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  g_set_prgname ("gtk-encode-symbolic-svg");

  context = g_option_context_new ("[OPTION…] PATH WIDTHxHEIGHT");
  g_option_context_add_main_entries (context, args, GETTEXT_PACKAGE);

  g_option_context_parse (context, &argc, &argv, NULL);

  if (argc < 3)
    {
      g_printerr ("%s\n", g_option_context_get_help (context, FALSE, NULL));
      return 1;
    }

  width = 0;
  height = 0;
  sizev = g_strsplit (argv[2], "x", 0);
  if (g_strv_length (sizev) == 2)
    {
      width = atoi(sizev[0]);
      height = atoi(sizev[1]);
    }
  g_strfreev (sizev);

  if (width == 0 || height == 0)
    {
      g_printerr (_("Invalid size %s\n"), argv[2]);
      return 1;
    }

  path = argv[1];
#ifdef G_OS_WIN32
  path = g_locale_to_utf8 (path, -1, NULL, NULL, NULL);
#endif

  basename = g_path_get_basename (path);

  symbolic = gdk_texture_new_from_filename_symbolic (path, width, height, &error);
  if (symbolic == NULL)
    {
      g_printerr (_("Can’t load file: %s\n"), error->message);
      return 1;
    }

  dot = strrchr (basename, '.');
  if (dot != NULL)
    *dot = 0;
  pngfile = g_strconcat (basename, ".symbolic.png", NULL);
  g_free (basename);

  if (output_dir != NULL)
    pngpath = g_build_filename (output_dir, pngfile, NULL);
  else
    pngpath = g_strdup (pngfile);

  g_free (pngfile);

  dest = g_file_new_for_path (pngpath);

  bytes = gdk_texture_save_to_png_bytes (symbolic);

  if (!g_file_replace_contents (dest,
                                g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes),
                                NULL,
                                FALSE,
                                G_FILE_CREATE_REPLACE_DESTINATION,
                                NULL,
                                NULL,
                                &error))
    {
      g_printerr (_("Can’t save file %s: %s\n"), pngpath, error->message);
      return 1;
    }

  g_bytes_unref (bytes);
  g_object_unref (dest);
  g_free (pngpath);

  return 0;
}

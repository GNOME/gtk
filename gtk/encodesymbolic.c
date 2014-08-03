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
#include <gdk-pixbuf/gdk-pixdata.h>
#include <gdk/gdk.h>
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

static gchar *output_dir = NULL;

static GOptionEntry args[] = {
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_dir, N_("Output to this directory instead of cwd"), NULL },
  { NULL }
};

static GdkPixbuf *
load_symbolic_svg (char *file_data, gsize file_len,
                   int width,
                   int height,
                   const GdkRGBA  *fg,
                   const GdkRGBA  *success_color,
                   const GdkRGBA  *warning_color,
                   const GdkRGBA  *error_color,
                   GError        **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;
  gchar *css_fg;
  gchar *css_success;
  gchar *css_warning;
  gchar *css_error;
  gchar *data;
  gchar *svg_width, *svg_height;
  gchar *escaped_file_data;

  css_fg = gdk_rgba_to_string (fg);

  css_success = css_warning = css_error = NULL;

  css_warning = gdk_rgba_to_string (warning_color);
  css_error = gdk_rgba_to_string (error_color);
  css_success = gdk_rgba_to_string (success_color);

  /* Fetch size from the original icon */
  stream = g_memory_input_stream_new_from_data (file_data, file_len, NULL);
  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, error);
  g_object_unref (stream);

  if (!pixbuf)
    return NULL;

  svg_width = g_strdup_printf ("%d", gdk_pixbuf_get_width (pixbuf));
  svg_height = g_strdup_printf ("%d",gdk_pixbuf_get_height (pixbuf));
  g_object_unref (pixbuf);

  escaped_file_data = g_markup_escape_text (file_data, file_len);

  data = g_strconcat ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                      "<svg version=\"1.1\"\n"
                      "     xmlns=\"http://www.w3.org/2000/svg\"\n"
                      "     xmlns:xi=\"http://www.w3.org/2001/XInclude\"\n"
                      "     width=\"", svg_width, "\"\n"
                      "     height=\"", svg_height, "\">\n"
                      "  <style type=\"text/css\">\n"
                      "    rect,path {\n"
                      "      fill: ", css_fg," !important;\n"
                      "    }\n"
                      "    .warning {\n"
                      "      fill: ", css_warning, " !important;\n"
                      "    }\n"
                      "    .error {\n"
                      "      fill: ", css_error ," !important;\n"
                      "    }\n"
                      "    .success {\n"
                      "      fill: ", css_success, " !important;\n"
                      "    }\n"
                      "  </style>\n"
                      "  <xi:include href=\"data:text/xml,", escaped_file_data, "\"/>\n"
                      "</svg>",
                      NULL);
  g_free (escaped_file_data);
  g_free (css_fg);
  g_free (css_warning);
  g_free (css_error);
  g_free (css_success);
  g_free (svg_width);
  g_free (svg_height);

  stream = g_memory_input_stream_new_from_data (data, -1, g_free);
  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
                                                width,
                                                height,
                                                TRUE,
                                                NULL,
                                                error);
  g_object_unref (stream);

  return pixbuf;
}

static void
extract_plane (GdkPixbuf *src,
               GdkPixbuf *dst,
               int from_plane,
               int to_plane)
{
  guchar *src_data, *dst_data;
  int width, height, src_stride, dst_stride;
  guchar *src_row, *dst_row;
  int x, y;

  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);

  g_assert (width <= gdk_pixbuf_get_width (dst));
  g_assert (height <= gdk_pixbuf_get_height (dst));

  src_stride = gdk_pixbuf_get_rowstride (src);
  src_data = gdk_pixbuf_get_pixels (src);

  dst_data = gdk_pixbuf_get_pixels (dst);
  dst_stride = gdk_pixbuf_get_rowstride (dst);

  for (y = 0; y < height; y++)
    {
      src_row = src_data + src_stride * y;
      dst_row = dst_data + dst_stride * y;
      for (x = 0; x < width; x++)
        {
          dst_row[to_plane] = src_row[from_plane];
          src_row += 4;
          dst_row += 4;
        }
    }
}

static GdkPixbuf *
make_symbolic_pixbuf (char *file,
                      int width,
                      int height,
                      GError        **error)

{
  GdkRGBA r = { 1,0,0,1}, g = {0,1,0,1};
  GdkPixbuf *loaded;
  GdkPixbuf *pixbuf;
  int plane;
  gchar *file_data;
  gsize file_len;

  if (!g_file_get_contents (file, &file_data, &file_len, error))
    return NULL;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);

  gdk_pixbuf_fill (pixbuf, 0);

  for (plane = 0; plane < 3; plane++)
    {
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
      loaded = load_symbolic_svg (file_data, file_len, width, height,
                                  &g,
                                  plane == 0 ? &r : &g,
                                  plane == 1 ? &r : &g,
                                  plane == 2 ? &r : &g,
                                  error);
      if (loaded == NULL)
        return NULL;

      if (plane == 0)
        extract_plane (loaded, pixbuf, 3, 3);

      extract_plane (loaded, pixbuf, 0, plane);

      g_object_unref (loaded);
    }

  g_free (file_data);

  return pixbuf;
}

int
main (int argc, char **argv)
{
  gchar *path, *basename, *pngpath, *pngfile, *dot;
  GOptionContext *context;
  GdkPixbuf *symbolic;
  GError *error;
  int width, height;
  gchar **sizev;
  GFileOutputStream *out;
  GFile *dest;

  setlocale (LC_ALL, "");

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
#endif

  g_set_prgname ("gtk-encode-symbolic-svg");

  context = g_option_context_new ("PATH WIDTHxHEIGHT");
  g_option_context_add_main_entries (context, args, GETTEXT_PACKAGE);

  g_option_context_parse (context, &argc, &argv, NULL);

  if (argc < 3)
    {
      g_printerr (g_option_context_get_help (context, FALSE, NULL));
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

  error = NULL;
  symbolic = make_symbolic_pixbuf (path, width, height, &error);
  if (symbolic == NULL)
    {
      g_printerr ("Can't load file: %s\n", error->message);
      return 1;
    }

  basename = g_path_get_basename (path);

  dot = strchr(basename, '.');
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


  out = g_file_replace (dest,
			NULL, FALSE,
			G_FILE_CREATE_REPLACE_DESTINATION,
			NULL, &error);
  if (out == NULL)
    {
      g_printerr ("Can't save file %s: %s\n", pngpath, error->message);
      return 1;
    }

  if (!gdk_pixbuf_save_to_stream (symbolic, G_OUTPUT_STREAM (out), "png", NULL, &error, NULL))
    {
      g_printerr ("Can't save file %s: %s\n", pngpath, error->message);
      return 1;
    }

  if (!g_output_stream_close (G_OUTPUT_STREAM (out), NULL, &error))
    {
      g_printerr ("Can't close stream");
      return 1;
    }

  g_object_unref (out);
  g_free (pngpath);

  return 0;
}

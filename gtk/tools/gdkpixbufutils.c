/* Copyright (C) 2016 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdk.h>
#include "gdkpixbufutilsprivate.h"

static GdkPixbuf *
load_from_stream (GdkPixbufLoader  *loader,
                  GInputStream     *stream,
                  GCancellable     *cancellable,
                  GError          **error)
{
  GdkPixbuf *pixbuf;
  gssize n_read;
  guchar buffer[65536];
  gboolean res;

  res = TRUE;
  while (1)
    {
      n_read = g_input_stream_read (stream, buffer, sizeof (buffer), cancellable, error);
      if (n_read < 0)
        {
          res = FALSE;
          error = NULL; /* Ignore further errors */
          break;
        }

      if (n_read == 0)
        break;

      if (!gdk_pixbuf_loader_write (loader, buffer, n_read, error))
        {
          res = FALSE;
          error = NULL;
          break;
        }
    }

  if (!gdk_pixbuf_loader_close (loader, error))
    {
      res = FALSE;
      error = NULL;
    }

  pixbuf = NULL;

  if (res)
    {
      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
      if (pixbuf)
        g_object_ref (pixbuf);
    }

  return pixbuf;
}

static void
size_prepared_cb (GdkPixbufLoader *loader,
                  gint             width,
                  gint             height,
                  gpointer         data)
{
  gdouble *scale = data;

  width = MAX (*scale * width, 1);
  height = MAX (*scale * height, 1);

  gdk_pixbuf_loader_set_size (loader, width, height);
}

/* Like gdk_pixbuf_new_from_stream_at_scale, but
 * load the image at its original size times the
 * given scale.
 */
GdkPixbuf *
_gdk_pixbuf_new_from_stream_scaled (GInputStream  *stream,
                                    gdouble        scale,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;

  loader = gdk_pixbuf_loader_new ();

  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_cb), &scale);

  pixbuf = load_from_stream (loader, stream, cancellable, error);

  g_object_unref (loader);

  return pixbuf;
}

/* Like gdk_pixbuf_new_from_resource_at_scale, but
 * load the image at its original size times the
 * given scale.
 */
GdkPixbuf *
_gdk_pixbuf_new_from_resource_scaled (const gchar  *resource_path,
                                      gdouble       scale,
                                      GError      **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;

  stream = g_resources_open_stream (resource_path, 0, error);
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_scaled (stream, scale, NULL, error);
  g_object_unref (stream);

  return pixbuf;
}

static GdkPixbuf *
load_symbolic_svg (const char     *file_data,
                   gsize           file_len,
                   int             width,
                   int             height,
                   double          scale,
                   int             icon_width,
                   int             icon_height,
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

  if (width == 0)
    width = icon_width * scale;
  if (height == 0)
    height = icon_height * scale;

  svg_width = g_strdup_printf ("%d", icon_width);
  svg_height = g_strdup_printf ("%d", icon_height);

  escaped_file_data = g_base64_encode ((guchar *) file_data, file_len);

  data = g_strconcat ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                      "<svg version=\"1.1\"\n"
                      "     xmlns=\"http://www.w3.org/2000/svg\"\n"
                      "     xmlns:xi=\"http://www.w3.org/2001/XInclude\"\n"
                      "     width=\"", svg_width, "\"\n"
                      "     height=\"", svg_height, "\">\n"
                      "  <style type=\"text/css\">\n"
                      "    rect,circle,path {\n"
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
                      "  <xi:include href=\"data:text/xml;base64,", escaped_file_data, "\"/>\n"
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
  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, width, height, TRUE, NULL, error);
  g_object_unref (stream);

  return pixbuf;
}

static void
extract_plane (GdkPixbuf *src,
               GdkPixbuf *dst,
               int        from_plane,
               int        to_plane)
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

GdkPixbuf *
gtk_make_symbolic_pixbuf_from_data (const char  *file_data,
                                    gsize        file_len,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    GError     **error)

{
  GdkRGBA r = { 1,0,0,1}, g = {0,1,0,1};
  GdkPixbuf *loaded;
  GdkPixbuf *pixbuf = NULL;
  int plane;
  int icon_width, icon_height;

  /* Fetch size from the original icon */
  {
    GInputStream *stream = g_memory_input_stream_new_from_data (file_data, file_len, NULL);
    GdkPixbuf *reference = gdk_pixbuf_new_from_stream (stream, NULL, error);

    g_object_unref (stream);

    if (!reference)
      return NULL;

    icon_width = gdk_pixbuf_get_width (reference);
    icon_height = gdk_pixbuf_get_height (reference);
    g_object_unref (reference);
  }

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
      loaded = load_symbolic_svg (file_data, file_len, width, height, scale,
                                  icon_width,
                                  icon_height,
                                  &g,
                                  plane == 0 ? &r : &g,
                                  plane == 1 ? &r : &g,
                                  plane == 2 ? &r : &g,
                                  error);
      if (loaded == NULL)
        return NULL;

      if (pixbuf == NULL)
        {
          pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                                   gdk_pixbuf_get_width (loaded),
                                   gdk_pixbuf_get_height (loaded));
          gdk_pixbuf_fill (pixbuf, 0);
        }

      if (plane == 0)
        extract_plane (loaded, pixbuf, 3, 3);

      extract_plane (loaded, pixbuf, 0, plane);

      g_object_unref (loaded);
    }

  return pixbuf;
}

GdkPixbuf *
gtk_make_symbolic_pixbuf_from_resource (const char  *path,
                                        int          width,
                                        int          height,
                                        double       scale,
                                        GError     **error)
{
  GBytes *bytes;
  const char *data;
  gsize size;
  GdkPixbuf *pixbuf;

  bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, error);
  if (bytes == NULL)
    return NULL;

  data = g_bytes_get_data (bytes, &size);

  pixbuf = gtk_make_symbolic_pixbuf_from_data (data, size, width, height, scale, error);

  g_bytes_unref (bytes);

  return pixbuf;
}

GdkPixbuf *
gtk_make_symbolic_pixbuf_from_file (GFile   *file,
                                    int      width,
                                    int      height,
                                    double   scale,
                                    GError **error)
{
  char *data;
  gsize size;
  GdkPixbuf *pixbuf;

  if (!g_file_load_contents (file, NULL, &data, &size, NULL, error))
    return NULL;

  pixbuf = gtk_make_symbolic_pixbuf_from_data (data, size, width, height, scale, error);

  g_free (data);

  return pixbuf;
}

GdkTexture *
gtk_make_symbolic_texture_from_resource (const char  *path,
                                         int          width,
                                         int          height,
                                         double       scale,
                                         GError     **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = gtk_make_symbolic_pixbuf_from_resource (path, width, height, scale, error);
  if (pixbuf)
    {
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }

  return texture;
}

GdkTexture *
gtk_make_symbolic_texture_from_file (GFile   *file,
                                     int      width,
                                     int      height,
                                     double   scale,
                                     GError **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;

  pixbuf = gtk_make_symbolic_pixbuf_from_file (file, width, height, scale, error);
  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

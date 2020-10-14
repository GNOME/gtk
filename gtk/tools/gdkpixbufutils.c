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
                  int              width,
                  int              height,
                  gpointer         data)
{
  double *scale = data;

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
                                    const char    *format,
                                    double         scale,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;

  if (format)
    {
      loader = gdk_pixbuf_loader_new_with_type (format, error);
      if (!loader)
        return NULL;
    }
  else
    loader = gdk_pixbuf_loader_new ();

  if (scale != 0)
    g_signal_connect (loader, "size-prepared",
                      G_CALLBACK (size_prepared_cb), &scale);

  pixbuf = load_from_stream (loader, stream, cancellable, error);

  g_object_unref (loader);

  return pixbuf;
}

static void
size_prepared_cb2 (GdkPixbufLoader *loader,
                   int              width,
                   int              height,
                   gpointer         data)
{
  int *scales = data;

  if (scales[2]) /* keep same aspect ratio as original, while fitting in given size */
    {
      double aspect = (double) height / width;

      /* First use given width and calculate size */
      width = scales[0];
      height = scales[0] * aspect;

      /* Check if it fits given height, otherwise scale down */
      if (height > scales[1])
        {
          width *= (double) scales[1] / height;
          height = scales[1];
        }
    }
  else
    {
      width = scales[0];
      height = scales[1];
    }

  gdk_pixbuf_loader_set_size (loader, width, height);
}

GdkPixbuf *
_gdk_pixbuf_new_from_stream_at_scale (GInputStream  *stream,
                                      const char    *format,
                                      int            width,
                                      int            height,
                                      gboolean       aspect,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;
  int scales[3];

  if (format)
    {
      loader = gdk_pixbuf_loader_new_with_type (format, error);
      if (!loader)
        return NULL;
    }
  else
    loader = gdk_pixbuf_loader_new ();

  scales[0] = width;
  scales[1] = height;
  scales[2] = aspect;
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_cb2), scales);

  pixbuf = load_from_stream (loader, stream, cancellable, error);

  g_object_unref (loader);

  return pixbuf;
}

GdkPixbuf *
_gdk_pixbuf_new_from_stream (GInputStream  *stream,
                             const char    *format,
                             GCancellable  *cancellable,
                             GError       **error)
{
  return _gdk_pixbuf_new_from_stream_scaled (stream, format, 0, cancellable, error);
}

/* Like gdk_pixbuf_new_from_resource_at_scale, but
 * load the image at its original size times the
 * given scale.
 */
GdkPixbuf *
_gdk_pixbuf_new_from_resource_scaled (const char  *resource_path,
                                      const char  *format,
                                      double       scale,
                                      GError     **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;

  stream = g_resources_open_stream (resource_path, 0, error);
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_scaled (stream, format, scale, NULL, error);
  g_object_unref (stream);

  return pixbuf;
}

GdkPixbuf *
_gdk_pixbuf_new_from_resource (const char   *resource_path,
                               const char   *format,
                               GError      **error)
{
  return _gdk_pixbuf_new_from_resource_scaled (resource_path, format, 0, error);
}

GdkPixbuf *
_gdk_pixbuf_new_from_resource_at_scale (const char   *resource_path,
                                        const char   *format,
                                        int           width,
                                        int           height,
                                        gboolean      preserve_aspect,
                                        GError      **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;

  stream = g_resources_open_stream (resource_path, 0, error);
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_at_scale (stream, format, width, height, preserve_aspect, NULL, error);
  g_object_unref (stream);

  return pixbuf;

}

static GdkPixbuf *
load_symbolic_svg (const char     *escaped_file_data,
                   int             width,
                   int             height,
                   double          scale,
                   int             icon_width,
                   int             icon_height,
                   const char     *fg_string,
                   const char     *success_color_string,
                   const char     *warning_color_string,
                   const char     *error_color_string,
                   GError        **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;
  char *data;
  char *svg_width, *svg_height;

  if (width == 0)
    width = icon_width * scale;
  if (height == 0)
    height = icon_height * scale;

  svg_width = g_strdup_printf ("%d", icon_width);
  svg_height = g_strdup_printf ("%d", icon_height);

  data = g_strconcat ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                      "<svg version=\"1.1\"\n"
                      "     xmlns=\"http://www.w3.org/2000/svg\"\n"
                      "     xmlns:xi=\"http://www.w3.org/2001/XInclude\"\n"
                      "     width=\"", svg_width, "\"\n"
                      "     height=\"", svg_height, "\">\n"
                      "  <style type=\"text/css\">\n"
                      "    rect,circle,path {\n"
                      "      fill: ", fg_string," !important;\n"
                      "    }\n"
                      "    .warning {\n"
                      "      fill: ", warning_color_string, " !important;\n"
                      "    }\n"
                      "    .error {\n"
                      "      fill: ", error_color_string ," !important;\n"
                      "    }\n"
                      "    .success {\n"
                      "      fill: ", success_color_string, " !important;\n"
                      "    }\n"
                      "  </style>\n"
                      "  <xi:include href=\"data:text/xml;base64,", escaped_file_data, "\"/>\n"
                      "</svg>",
                      NULL);
  g_free (svg_width);
  g_free (svg_height);

  stream = g_memory_input_stream_new_from_data (data, -1, g_free);
  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, width, height, TRUE, NULL, error);
  g_object_unref (stream);

  return pixbuf;
}

static void
rgba_to_pixel (const GdkRGBA *rgba,
               guint8         pixel[4])
{
  pixel[0] = rgba->red * 255;
  pixel[1] = rgba->green * 255;
  pixel[2] = rgba->blue * 255;
  pixel[3] = 255;
}

GdkPixbuf *
gtk_color_symbolic_pixbuf (GdkPixbuf     *symbolic,
                           const GdkRGBA *fg_color,
                           const GdkRGBA *success_color,
                           const GdkRGBA *warning_color,
                           const GdkRGBA *error_color)
{
  int width, height, x, y, src_stride, dst_stride;
  guchar *src_data, *dst_data;
  guchar *src_row, *dst_row;
  int alpha;
  GdkPixbuf *colored;
  guint8 fg_pixel[4], success_pixel[4], warning_pixel[4], error_pixel[4];

  alpha = fg_color->alpha * 255;

  rgba_to_pixel (fg_color, fg_pixel);
  rgba_to_pixel (success_color, success_pixel);
  rgba_to_pixel (warning_color, warning_pixel);
  rgba_to_pixel (error_color, error_pixel);

  width = gdk_pixbuf_get_width (symbolic);
  height = gdk_pixbuf_get_height (symbolic);

  colored = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);

  src_stride = gdk_pixbuf_get_rowstride (symbolic);
  src_data = gdk_pixbuf_get_pixels (symbolic);

  dst_data = gdk_pixbuf_get_pixels (colored);
  dst_stride = gdk_pixbuf_get_rowstride (colored);
  for (y = 0; y < height; y++)
    {
      src_row = src_data + src_stride * y;
      dst_row = dst_data + dst_stride * y;
      for (x = 0; x < width; x++)
        {
          guint r, g, b, a;
          int c1, c2, c3, c4;

          a = src_row[3];
          dst_row[3] = a * alpha / 255;

          if (a == 0)
            {
              dst_row[0] = 0;
              dst_row[1] = 0;
              dst_row[2] = 0;
            }
          else
            {
              c2 = src_row[0];
              c3 = src_row[1];
              c4 = src_row[2];

              if (c2 == 0 && c3 == 0 && c4 == 0)
                {
                  dst_row[0] = fg_pixel[0];
                  dst_row[1] = fg_pixel[1];
                  dst_row[2] = fg_pixel[2];
                }
              else
                {
                  c1 = 255 - c2 - c3 - c4;

                  r = fg_pixel[0] * c1 + success_pixel[0] * c2 +  warning_pixel[0] * c3 +  error_pixel[0] * c4;
                  g = fg_pixel[1] * c1 + success_pixel[1] * c2 +  warning_pixel[1] * c3 +  error_pixel[1] * c4;
                  b = fg_pixel[2] * c1 + success_pixel[2] * c2 +  warning_pixel[2] * c3 +  error_pixel[2] * c4;

                  dst_row[0] = r / 255;
                  dst_row[1] = g / 255;
                  dst_row[2] = b / 255;
                }
            }

          src_row += 4;
          dst_row += 4;
        }
    }

  return colored;
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
  const char *r_string = "rgb(255,0,0)";
  const char *g_string = "rgb(0,255,0)";
  GdkPixbuf *loaded;
  GdkPixbuf *pixbuf = NULL;
  int plane;
  int icon_width, icon_height;
  char *escaped_file_data;

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

  escaped_file_data = g_base64_encode ((guchar *) file_data, file_len);

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
      loaded = load_symbolic_svg (escaped_file_data, width, height, scale,
                                  icon_width,
                                  icon_height,
                                  g_string,
                                  plane == 0 ? r_string : g_string,
                                  plane == 1 ? r_string : g_string,
                                  plane == 2 ? r_string : g_string,
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

  g_free (escaped_file_data);

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
gtk_make_symbolic_pixbuf_from_path (const char  *path,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    GError     **error)
{
  char *data;
  gsize size;
  GdkPixbuf *pixbuf;

  if (!g_file_get_contents (path, &data, &size, error))
    return NULL;

  pixbuf = gtk_make_symbolic_pixbuf_from_data (data, size, width, height, scale, error);

  g_free (data);

  return pixbuf;
}

GdkPixbuf *
gtk_make_symbolic_pixbuf_from_file (GFile       *file,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    GError     **error)
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
gtk_load_symbolic_texture_from_resource (const char *path)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;

  pixbuf = _gdk_pixbuf_new_from_resource (path, "png", NULL);
  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
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
gtk_load_symbolic_texture_from_file (GFile *file)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  GInputStream *stream;

  stream = G_INPUT_STREAM (g_file_read (file, NULL, NULL));
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream (stream, "png", NULL, NULL);
  g_object_unref (stream);
  if (pixbuf == NULL)
    return NULL;

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

GdkTexture *
gtk_make_symbolic_texture_from_file (GFile       *file,
                                     int          width,
                                     int          height,
                                     double       scale,
                                     GError     **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;

  pixbuf = gtk_make_symbolic_pixbuf_from_file (file, width, height, scale, error);
  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

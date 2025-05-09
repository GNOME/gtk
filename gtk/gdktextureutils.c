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
#include "gdktextureutilsprivate.h"
#include "gtkscalerprivate.h"
#include "gtksnapshot.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gdk/gdktextureprivate.h"
#include "gdk/loaders/gdkpngprivate.h"
#include "gdk/gdkdebugprivate.h"
#include "gtk/gtkdebug.h"


/* {{{ Pixbuf helpers */

static inline gboolean
pixbuf_is_only_fg (GdkPixbuf *pixbuf)
{
  return gdk_pixbuf_get_option (pixbuf, "tEXt::only-foreground") != NULL;
}

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
static GdkPixbuf *
_gdk_pixbuf_new_from_stream_scaled (GInputStream  *stream,
                                    double         scale,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;

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

static GdkPixbuf *
_gdk_pixbuf_new_from_stream_at_scale (GInputStream  *stream,
                                      int            width,
                                      int            height,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;
  int scales[3];

  loader = gdk_pixbuf_loader_new ();

  scales[0] = width;
  scales[1] = height;
  scales[2] = TRUE;
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_cb2), scales);

  pixbuf = load_from_stream (loader, stream, cancellable, error);

  g_object_unref (loader);

  return pixbuf;
}

static GdkPixbuf *
_gdk_pixbuf_new_from_resource_at_scale (const char   *resource_path,
                                        int           width,
                                        int           height,
                                        GError      **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;

  stream = g_resources_open_stream (resource_path, 0, error);
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_at_scale (stream, width, height, NULL, error);
  g_object_unref (stream);

  return pixbuf;
}

/* }}} */
/* {{{ Symbolic processing */

static GdkTexture *
load_symbolic_svg (const char     *escaped_file_data,
                   gsize           len,
                   int             width,
                   int             height,
                   const char     *icon_width_str,
                   const char     *icon_height_str,
                   const char     *fg_string,
                   const char     *success_color_string,
                   const char     *warning_color_string,
                   const char     *error_color_string,
                   GError        **error)
{
  GInputStream *stream;
  GdkTexture *texture;
  char *data;

  data = g_strconcat ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                      "<svg version=\"1.1\" "
                           "xmlns=\"http://www.w3.org/2000/svg\" "
                           "xmlns:xi=\"http://www.w3.org/2001/XInclude\" "
                           "width=\"", icon_width_str, "\" "
                           "height=\"", icon_height_str, "\">"
                        "<style type=\"text/css\">"
                          "rect,circle,path,.foreground-fill {"
                            "fill: ", fg_string," !important;"
                          "}\n"
                          ".warning,.warning-fill {"
                            "fill: ", warning_color_string, " !important;"
                          "}\n"
                          ".error,.error-fill {"
                            "fill: ", error_color_string ," !important;"
                          "}\n"
                          ".success,.success-fill {"
                            "fill: ", success_color_string, " !important;"
                          "}\n"
                          ".transparent-fill {"
                            "fill: none !important;"
                          "}\n"
                          ".foreground-stroke {"
                            "stroke: ", fg_string," !important;"
                          "}\n"
                          ".warning-stroke {"
                            "stroke: ", warning_color_string, " !important;"
                          "}\n"
                          ".error-stroke {"
                            "stroke: ", error_color_string ," !important;"
                          "}\n"
                          ".success-stroke {"
                            "stroke: ", success_color_string, " !important;"
                          "}"
                        "</style>"
                        "<xi:include href=\"data:text/xml;base64,",
                      NULL);

  stream = g_memory_input_stream_new_from_data (data, -1, g_free);
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream), escaped_file_data, len, NULL);
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream), "\"/></svg>", strlen ("\"/></svg>"), NULL);
  texture = gdk_texture_new_from_stream_at_scale (stream, width, height, NULL, NULL, error);
  g_object_unref (stream);

  return texture;
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

static void
svg_find_size_strings (const char  *data,
                       gsize        len,
                       char       **width,
                       char       **height)
{
  gsize i, j, k, l;

  *width = NULL;
  *height = NULL;

  for (i = 0; i < len - 4; i++)
    {
      if (strncmp (data + i, "<svg", 4) == 0)
        {
          for (j = i + strlen ("<svg"); j < len - 9; j++)
            {
              if (strncmp (data + j, "height=\"", strlen ("height=\"")) == 0)
                {
                  k = l = j + strlen ("height=\"");
                  while (l < len && data[l] != '\"')
                    l++;

                  *height = g_strndup (data + k, l - k);

                  if (*width && *height)
                    return;

                  j = l;
                }
              else if (strncmp (data + j, "width=\"", strlen ("width=\"")) == 0)
                {
                  k = l = j + strlen ("width=\"");
                  while (l < len && data[l] != '\"')
                    l++;

                  *width = g_strndup (data + k, l - k);

                  if (*width && *height)
                    return;

                  j = l;
                }
              else if (data[j] == '>')
                {
                  break;
                }
            }

          break;
        }
    }

  *width = g_strdup ("16px");
  *height = g_strdup ("16px");
}

static gboolean
svg_has_symbolic_classes (const char *data,
                          gsize       len)
{
#ifdef HAVE_MEMMEM
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
                                     gboolean  *out_only_fg,
                                     GError   **error)

{
  const char *file_data;
  gsize file_len;
  const char *r_string = "rgb(255,0,0)";
  const char *g_string = "rgb(0,255,0)";
  char *icon_width_str = NULL;
  char *icon_height_str = NULL;
  char *escaped_file_data = NULL;
  gsize len;
  gboolean has_symbolic_classes;
  gboolean only_fg = TRUE;
  guchar *data;
  GBytes *data_bytes;
  GdkTexture *texture;

  file_data = g_bytes_get_data (bytes, &file_len);

  has_symbolic_classes = svg_has_symbolic_classes (file_data, file_len);

  /* Fetch size from the original icon */
  if (has_symbolic_classes || width == 0 || height == 0)
    svg_find_size_strings (file_data, file_len, &icon_width_str, &icon_height_str);

  if (width == 0 || height == 0)
    {
      width = (int) g_ascii_strtoull (icon_width_str, NULL, 0);
      height = (int) g_ascii_strtoull (icon_height_str, NULL, 0);
    }

  if (!has_symbolic_classes)
    {
      GInputStream *stream;

      stream = g_memory_input_stream_new_from_data (file_data, file_len, NULL);
      texture = gdk_texture_new_from_stream_at_scale (stream, width, height, NULL, NULL, error);
      g_object_unref (stream);

      if (texture)
        texture = keep_alpha (texture);

      goto out;
    }

  texture = NULL;

  escaped_file_data = g_base64_encode ((guchar *) file_data, file_len);
  len = strlen (escaped_file_data);

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
      loaded = load_symbolic_svg (escaped_file_data, len, width, height,
                                  icon_width_str,
                                  icon_height_str,
                                  g_string,
                                  plane == 0 ? r_string : g_string,
                                  plane == 1 ? r_string : g_string,
                                  plane == 2 ? r_string : g_string,
                                  error);
      if (loaded == NULL)
        goto out;

#ifdef DEBUG_SYMBOLIC
        {
          static int debug_counter;
          char *filename;

          if (plane == 0)
            debug_counter++;

          filename = g_strdup_printf ("image%d.debug%d.png", debug_counter, plane);
          g_print ("Writing %s\n", filename);
          gdk_texture_save_to_png (loaded, filename);
          g_free (filename);
        }
#endif

      if (plane == 0)
        {
          data = g_new0 (guchar, 4 * width * height);
          extract_plane (loaded, data, width, height, 3, 3);
        }

      only_fg &= extract_plane (loaded, data, width, height, 0, plane);

      g_object_unref (loaded);
    }

  data_bytes = g_bytes_new_take (data, 4 * width * height);
  texture = gdk_memory_texture_new (width, height,
                                    GDK_MEMORY_R8G8B8A8,
                                    data_bytes,
                                    4 * width);
  g_bytes_unref (data_bytes);

out:
  g_free (escaped_file_data);
  g_free (icon_width_str);
  g_free (icon_height_str);

  if (out_only_fg)
    *out_only_fg = only_fg;

  return texture;
}

/* }}} */
/* {{{ Texture API */

static GdkTexture *
gdk_texture_new_from_bytes_with_fg (GBytes    *bytes,
                                    gboolean  *only_fg,
                                    GError   **error)
{
  GHashTable *options;
  GdkTexture *texture;

  if (!gdk_is_png (bytes))
    {
      if (only_fg)
        *only_fg = FALSE;
      return gdk_texture_new_from_bytes (bytes, error);
    }

  options = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  texture = gdk_load_png (bytes, options, error);
  if (only_fg)
    *only_fg = g_hash_table_contains (options, "only-foreground");
  g_hash_table_unref (options);

  return texture;
}

GdkTexture *
gdk_texture_new_from_filename_with_fg (const char    *filename,
                                       gboolean      *only_fg,
                                       GError       **error)
{
  GFile *file;
  GBytes *bytes;
  GdkTexture *texture = NULL;

  file = g_file_new_for_path (filename);
  bytes = g_file_load_bytes (file, NULL, NULL, error);
  if (bytes)
    texture = gdk_texture_new_from_bytes_with_fg (bytes, only_fg, error);
  g_bytes_unref (bytes);
  g_object_unref (file);

  return texture;
}

GdkTexture *
gdk_texture_new_from_resource_with_fg (const char *path,
                                       gboolean   *only_fg)
{
  GBytes *bytes;
  GdkTexture *texture = NULL;

  bytes = g_resources_lookup_data (path, 0, NULL);
  if (bytes)
    texture = gdk_texture_new_from_bytes_with_fg (bytes, only_fg, NULL);
  g_bytes_unref (bytes);

  return texture;
}

GdkTexture *
gdk_texture_new_from_stream_with_fg (GInputStream  *stream,
                                     gboolean      *only_fg,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_scaled (stream, 0, cancellable, error);
  if (pixbuf)
    {
      if (only_fg)
        *only_fg = pixbuf_is_only_fg (pixbuf);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      texture = gdk_texture_new_for_pixbuf (pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS
      g_object_unref (pixbuf);
    }

  return texture;
}

GdkTexture *
gdk_texture_new_from_stream_at_scale (GInputStream  *stream,
                                      int            width,
                                      int            height,
                                      gboolean      *only_fg,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_at_scale (stream, width, height, cancellable, error);
  if (pixbuf)
    {
      if (only_fg)
        *only_fg = pixbuf_is_only_fg (pixbuf);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      texture = gdk_texture_new_for_pixbuf (pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS
      g_object_unref (pixbuf);
    }

  return texture;
}

GdkTexture *
gdk_texture_new_from_resource_at_scale (const char    *path,
                                        int            width,
                                        int            height,
                                        gboolean      *only_fg,
                                        GError       **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = _gdk_pixbuf_new_from_resource_at_scale (path, width, height, error);
  if (pixbuf)
    {
      if (only_fg)
        *only_fg = pixbuf_is_only_fg (pixbuf);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      texture = gdk_texture_new_for_pixbuf (pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS
      g_object_unref (pixbuf);
    }

  return texture;
}

GdkTexture *
gdk_texture_new_from_filename_at_scale (const char  *filename,
                                        int          width,
                                        int          height,
                                        gboolean    *only_fg,
                                        GError     **error)
{
  GFile *file;
  GInputStream *stream;
  GdkTexture *texture;

  file = g_file_new_for_path (filename);
  stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
  g_object_unref (file);

  if (!stream)
    return NULL;

  texture = gdk_texture_new_from_stream_at_scale (stream, width, height, only_fg, NULL, error);
  g_object_unref (stream);

  return texture;
}

/* }}} */
/* {{{ Symbolic texture API */

GdkTexture *
gdk_texture_new_from_filename_symbolic (const char  *filename,
                                        int          width,
                                        int          height,
                                        gboolean    *only_fg,
                                        GError     **error)
{
  GFile *file;
  GdkTexture *texture;

  file = g_file_new_for_path (filename);
  texture = gdk_texture_new_from_file_symbolic (file,
                                                width, height,
                                                only_fg,
                                                error);
  g_object_unref (file);

  return texture;
}

GdkTexture *
gdk_texture_new_from_resource_symbolic (const char  *path,
                                        int          width,
                                        int          height,
                                        gboolean    *only_fg,
                                        GError     **error)
{
  GBytes *bytes;
  GdkTexture *texture;

  bytes = g_resources_lookup_data (path, 0, error);
  if (!bytes)
    return NULL;

  texture = gdk_texture_new_from_bytes_symbolic (bytes,
                                                 width, height,
                                                 only_fg,
                                                 error);

  g_bytes_unref (bytes);

  return texture;

}

GdkTexture *
gdk_texture_new_from_file_symbolic (GFile     *file,
                                    int        width,
                                    int        height,
                                    gboolean  *only_fg,
                                    GError   **error)
{
  GBytes *bytes;
  GdkTexture *texture;

  bytes = g_file_load_bytes (file, NULL, NULL, error);
  if (!bytes)
    return NULL;

  texture = gdk_texture_new_from_bytes_symbolic (bytes,
                                                 width, height,
                                                 only_fg,
                                                 error);

  g_bytes_unref (bytes);

  return texture;
}

/* }}} */
/* {{{ Scaled paintable API */

typedef struct {
  double scale;
} LoaderData;

static void
on_loader_size_prepared (GdkPixbufLoader *loader,
                         int              width,
                         int              height,
                         gpointer         user_data)
{
  LoaderData *loader_data = user_data;
  GdkPixbufFormat *format;

  /* Let the regular icon helper code path handle non-scalable images */
  format = gdk_pixbuf_loader_get_format (loader);
  if (!gdk_pixbuf_format_is_scalable (format))
    {
      loader_data->scale = 1.0;
      return;
    }

  gdk_pixbuf_loader_set_size (loader,
                              width * loader_data->scale,
                              height * loader_data->scale);
}

static GdkPaintable *
gdk_paintable_new_from_bytes_scaled (GBytes *bytes,
                                     double  scale)
{
  if (gdk_texture_can_load (bytes))
    {
      /* We know these formats can't be scaled */
      return GDK_PAINTABLE (gdk_texture_new_from_bytes (bytes, NULL));
    }
  else
    {
      GdkPixbufLoader *loader;
      gboolean success;
      GdkTexture *texture;
      GdkPaintable *paintable;
      LoaderData loader_data;

      loader_data.scale = scale;

      loader = gdk_pixbuf_loader_new ();
      g_signal_connect (loader, "size-prepared",
                        G_CALLBACK (on_loader_size_prepared), &loader_data);

      success = gdk_pixbuf_loader_write_bytes (loader, bytes, NULL);
      /* close even when writing failed */
      success &= gdk_pixbuf_loader_close (loader, NULL);

      if (!success)
        {
          g_object_unref (loader);
          return NULL;
        }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      texture = gdk_texture_new_for_pixbuf (gdk_pixbuf_loader_get_pixbuf (loader));
G_GNUC_END_IGNORE_DEPRECATIONS

      g_object_unref (loader);

      if (loader_data.scale != 1.0)
        paintable = gtk_scaler_new (GDK_PAINTABLE (texture), loader_data.scale);
      else
        paintable = g_object_ref (GDK_PAINTABLE (texture));

      g_object_unref (texture);

      return paintable;
    }
}

GdkPaintable *
gdk_paintable_new_from_filename_scaled (const char *filename,
                                        double      scale)
{
  char *contents;
  gsize length;
  GBytes *bytes;
  GdkPaintable *paintable;

  if (!g_file_get_contents (filename, &contents, &length, NULL))
    return NULL;

  bytes = g_bytes_new_take (contents, length);

  paintable = gdk_paintable_new_from_bytes_scaled (bytes, scale);

  g_bytes_unref (bytes);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_resource_scaled (const char *path,
                                        double      scale)
{
  GBytes *bytes;
  GdkPaintable *paintable;

  bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (!bytes)
    return NULL;

  paintable = gdk_paintable_new_from_bytes_scaled (bytes, scale);

  g_bytes_unref (bytes);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_file_scaled (GFile  *file,
                                    double  scale)
{
  GBytes *bytes;
  GdkPaintable *paintable;

  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (!bytes)
    return NULL;

  paintable = gdk_paintable_new_from_bytes_scaled (bytes, scale);

  g_bytes_unref (bytes);

  return paintable;
}

/* }}} */
/* {{{ Render node API */

typedef struct
{
  double width, height;
  GtkSnapshot *snapshot;
  gboolean only_fg;
  gboolean has_clip;
} ParserData;

static void
set_attribute_error (GError     **error,
                     const char  *name,
                     const char  *value)
{
  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
               "Could not handle %s attribute: %s", name, value);
}

static void
start_element_cb (GMarkupParseContext  *context,
                  const gchar          *element_name,
                  const gchar         **attribute_names,
                  const gchar         **attribute_values,
                  gpointer              user_data,
                  GError              **error)
{
  ParserData *data = user_data;

  if (strcmp (element_name, "svg") == 0)
    {
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      char *end;

      for (int i = 0; attribute_names[i]; i++)
        {
          if (strcmp (attribute_names[i], "width") == 0)
            width_attr = attribute_values[i];
          else if (strcmp (attribute_names[i], "height") == 0)
            height_attr = attribute_values[i];
        }

      if (!width_attr)
        {
          g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                               "Missing attribute: width");
          return;
        }
      data->width = g_ascii_strtod (width_attr, &end);
      if (end && *end != '\0' && strcmp (end, "px") != 0)
        {
          set_attribute_error (error, "width", width_attr);
          return;
        }

      if (!height_attr)
        {
          g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                               "Missing attribute: height");
          return;
        }

      data->height = g_ascii_strtod (height_attr, &end);
      if (end && *end != '\0' && strcmp (end, "px") != 0)
        {
          set_attribute_error (error, "height", height_attr);
          return;
        }

      gtk_snapshot_push_clip (data->snapshot, &GRAPHENE_RECT_INIT (0, 0, data->width, data->height));
      data->has_clip = TRUE;
    }
  else if (strcmp (element_name, "g") == 0)
    {
      /* Do nothing */
    }
  else if (strcmp (element_name, "path") == 0)
    {
      const char *path_attr = NULL;
      const char *fill_rule_attr = NULL;
      const char *fill_opacity_attr = NULL;
      const char *stroke_width_attr = NULL;
      const char *stroke_opacity_attr = NULL;
      const char *stroke_linecap_attr = NULL;
      const char *stroke_linejoin_attr = NULL;
      const char *stroke_miterlimit_attr = NULL;
      const char *stroke_dasharray_attr = NULL;
      const char *stroke_dashoffset_attr = NULL;
      const char *opacity_attr = NULL;
      const char *class_attr = NULL;
      GskPath *path = NULL;
      GskStroke *stroke = NULL;
      GskFillRule fill_rule;
      double opacity;
      double fill_opacity;
      double stroke_opacity;
      GdkRGBA fill_color;
      GdkRGBA stroke_color;
      char *end;
      gboolean do_fill = FALSE;
      gboolean do_stroke = FALSE;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "d", &path_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "class", &class_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "opacity", &opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill-rule", &fill_rule_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill-opacity", &fill_opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-width", &stroke_width_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-opacity", &stroke_opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-linecap", &stroke_linecap_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-linejoin", &stroke_linejoin_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-miterlimit", &stroke_miterlimit_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-dasharray", &stroke_dasharray_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "stroke-dashoffset", &stroke_dashoffset_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "style", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "id", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "color", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "overflow", NULL,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      path = gsk_path_parse (path_attr);
      if (!path)
        {
          set_attribute_error (error, "d", path_attr);
          return;
        }

      fill_opacity = 1;
      if (fill_opacity_attr)
        {
          fill_opacity = g_ascii_strtod (fill_opacity_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "fill-opacity", fill_opacity_attr);
              goto cleanup;
            }
          fill_opacity = CLAMP (fill_opacity, 0, 1);
        }

      stroke_opacity = 1;
      if (stroke_opacity_attr)
        {
          stroke_opacity = g_ascii_strtod (stroke_opacity_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "stroke-opacity", stroke_opacity_attr);
              goto cleanup;
            }
          stroke_opacity = CLAMP (stroke_opacity, 0, 1);
        }

      if (!class_attr)
        {
          fill_color = (GdkRGBA) { 0, 0, 0, fill_opacity };
          do_fill = TRUE;
          do_stroke = FALSE;
        }
      else
        {
          const char * const *classes;

          classes = (const char * const *) g_strsplit (class_attr, " ", 0);

          if (g_strv_contains (classes, "transparent-fill"))
            {
              do_fill = FALSE;
              data->only_fg = FALSE;
            }
          else if (g_strv_contains (classes, "foreground-fill"))
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 0, 0, 0, fill_opacity };
            }
          else if (g_strv_contains (classes, "success") ||
                   g_strv_contains (classes, "success-fill"))
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 1, 0, 0, fill_opacity };
              data->only_fg = FALSE;
            }
          else if (g_strv_contains (classes, "warning") ||
                   g_strv_contains (classes, "warning-fill"))
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 0, 1, 0, 1 };
              data->only_fg = FALSE;
            }
          else if (g_strv_contains (classes, "error") ||
                   g_strv_contains (classes, "error-fill"))
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 0, 0, 1, fill_opacity };
              data->only_fg = FALSE;
            }
          else
            {
              do_fill = TRUE;
              fill_color = (GdkRGBA) { 0, 0, 0, fill_opacity };
            }

          if (g_strv_contains (classes, "success-stroke"))
            {
              do_stroke = TRUE;
              stroke_color = (GdkRGBA) { 1, 0, 0, stroke_opacity };
              data->only_fg = FALSE;
            }
          else if (g_strv_contains (classes, "warning-stroke"))
            {
              do_stroke = TRUE;
              stroke_color = (GdkRGBA) { 0, 1, 0, stroke_opacity };
              data->only_fg = FALSE;
            }
          else if (g_strv_contains (classes, "error-stroke"))
            {
              do_stroke = TRUE;
              stroke_color = (GdkRGBA) { 0, 0, 1, stroke_opacity };
              data->only_fg = FALSE;
            }
          else if (g_strv_contains (classes, "foreground-stroke"))
            {
              do_stroke = TRUE;
              stroke_color = (GdkRGBA) { 0, 0, 0, stroke_opacity };
            }
          else
            {
              do_stroke = FALSE;
            }

          g_strfreev ((char **) classes);
        }

      opacity = 1;
      if (opacity_attr)
        {
          opacity = g_ascii_strtod (opacity_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "opacity", opacity_attr);
              goto cleanup;
            }
        }

      if (fill_rule_attr && strcmp (fill_rule_attr, "evenodd") == 0)
        fill_rule = GSK_FILL_RULE_EVEN_ODD;
      else
        fill_rule = GSK_FILL_RULE_WINDING;

      stroke = gsk_stroke_new (1);

      if (stroke_width_attr)
        {
          double w = g_ascii_strtod (stroke_width_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "stroke-width", stroke_width_attr);
              goto cleanup;
            }

          gsk_stroke_set_line_width (stroke, w);
        }

      if (stroke_linecap_attr)
        {
          if (strcmp (stroke_linecap_attr, "butt") == 0)
            gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_BUTT);
          else if (strcmp (stroke_linecap_attr, "round") == 0)
            gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);
          else if (strcmp (stroke_linecap_attr, "square") == 0)
            gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_SQUARE);
          else
            {
              set_attribute_error (error, "stroke-linecap", stroke_linecap_attr);
              goto cleanup;
            }
        }

      if (stroke_linejoin_attr)
        {
          if (strcmp (stroke_linejoin_attr, "miter") == 0)
            gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_MITER);
          else if (strcmp (stroke_linejoin_attr, "round") == 0)
            gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_ROUND);
          else if (strcmp (stroke_linejoin_attr, "bevel") == 0)
            gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_BEVEL);
          else
            {
              set_attribute_error (error, "stroke-linejoin", stroke_linejoin_attr);
              goto cleanup;
            }
        }

      if (stroke_miterlimit_attr)
        {
          double ml = g_ascii_strtod (stroke_miterlimit_attr, &end);
          if ((end && *end != '\0') || ml < 1)
            {
              set_attribute_error (error, "stroke-miterlimit", stroke_miterlimit_attr);
              goto cleanup;
            }

          gsk_stroke_set_miter_limit (stroke, ml);
        }

      if (stroke_dasharray_attr &&
          strcmp (stroke_dasharray_attr, "none") != 0)
        {
          char **str;
          gsize n_dash;

          str = g_strsplit_set (stroke_dasharray_attr, ", ", 0);

          n_dash = g_strv_length (str);
          if (n_dash > 0)
            {
              float *dash = g_newa (float, n_dash);

              for (int i = 0; i < n_dash; i++)
                {
                  dash[i] = g_ascii_strtod (str[i], &end);
                  if (end && *end != '\0')
                    {
                      set_attribute_error (error, "stroke-dasharray", stroke_dasharray_attr);
                      g_strfreev (str);
                      goto cleanup;
                    }
                }

              gsk_stroke_set_dash (stroke, dash, n_dash);
            }

          g_strfreev (str);
        }

      if (stroke_dashoffset_attr)
        {
          double offset = g_ascii_strtod (stroke_dashoffset_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "stroke-dashoffset", stroke_dashoffset_attr);
              goto cleanup;
            }

          gsk_stroke_set_dash_offset (stroke, offset);
        }

      if (opacity != 1)
        gtk_snapshot_push_opacity (data->snapshot, opacity);

      if (do_fill)
        gtk_snapshot_append_fill (data->snapshot, path, fill_rule, &fill_color);

      if (do_stroke)
        gtk_snapshot_append_stroke (data->snapshot, path, stroke, &stroke_color);

      if (opacity != 1)
        gtk_snapshot_pop (data->snapshot);

cleanup:
      g_clear_pointer (&path, gsk_path_unref);
      g_clear_pointer (&stroke, gsk_stroke_free);
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Unhandled element: %s", element_name);
    }
}

static void
end_element_cb (GMarkupParseContext *context,
                const gchar         *element_name,
                gpointer             user_data,
                GError             **error)
{
  ParserData *data = user_data;

  if (strcmp (element_name, "svg") == 0)
    {
      if (data->has_clip)
        {
          gtk_snapshot_pop (data->snapshot);
          data->has_clip = FALSE;
        }
    }
}

static GskRenderNode *
gsk_render_node_new_from_bytes_symbolic (GBytes   *bytes,
                                         gboolean *only_fg,
                                         double   *width,
                                         double   *height)
{
  GMarkupParseContext *context;
  GMarkupParser parser = {
    start_element_cb,
    end_element_cb,
    NULL,
    NULL,
    NULL,
  };
  ParserData data;
  GError *error = NULL;
  const char *text;
  gsize len;

  data.width = data.height = 0;
  data.only_fg = TRUE;
  data.snapshot = gtk_snapshot_new ();
  data.has_clip = FALSE;

  text = g_bytes_get_data (bytes, &len);

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  if (!g_markup_parse_context_parse (context, text, len, &error))
    {
      GskRenderNode *node;

      if (GTK_DEBUG_CHECK (ICONTHEME))
        gdk_debug_message ("Failed to convert svg to node: %s", error->message);

      g_error_free (error);

      g_markup_parse_context_free (context);
      if (data.has_clip)
        gtk_snapshot_pop (data.snapshot);
      node = gtk_snapshot_free_to_node (data.snapshot);
      g_clear_pointer (&node, gsk_render_node_unref);

      return NULL;
    }

  g_markup_parse_context_free (context);

  if (only_fg)
    *only_fg = data.only_fg;

  *width = data.width;
  *height = data.height;

  return gtk_snapshot_free_to_node (data.snapshot);
}

GskRenderNode *
gsk_render_node_new_from_resource_symbolic (const char *path,
                                            gboolean   *only_fg,
                                            double     *width,
                                            double     *height)
{
  GBytes *bytes;
  GskRenderNode *node;

  if (!gdk_has_feature (GDK_FEATURE_ICON_NODES))
    return NULL;

  bytes = g_resources_lookup_data (path, 0, NULL);
  if (!bytes)
    return NULL;

  node = gsk_render_node_new_from_bytes_symbolic (bytes, only_fg, width, height);
  g_bytes_unref (bytes);

  return node;
}

GskRenderNode *
gsk_render_node_new_from_filename_symbolic (const char *filename,
                                            gboolean   *only_fg,
                                            double     *width,
                                            double     *height)
{
  char *text;
  gsize len;
  GBytes *bytes;
  GskRenderNode *node;

  if (!gdk_has_feature (GDK_FEATURE_ICON_NODES))
    return NULL;

  if (!g_file_get_contents (filename, &text, &len, NULL))
    return NULL;

  bytes = g_bytes_new_take (text, len);
  node = gsk_render_node_new_from_bytes_symbolic (bytes, only_fg, width, height);
  g_bytes_unref (bytes);

  return node;
}

/* }}} */

/* vim:set foldmethod=marker: */

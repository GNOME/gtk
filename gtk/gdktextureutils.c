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

#include <gsk/gsk.h>
#include "gdktextureutilsprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gtksnapshot.h"
#include "gtksvg.h"
#include "gtksymbolicpaintable.h"
#include "gtkprivate.h"

/* {{{ svg helpers */

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

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static void
svg_load_error (GtkSvg       *svg,
                const GError *error,
                gpointer      user_data)
{
  char **unsupported = user_data;

  if (g_error_matches (error, GTK_SVG_ERROR, GTK_SVG_ERROR_NOT_IMPLEMENTED))
    {
      if (unsupported && !*unsupported)
        *unsupported = g_strdup (error->message);
    }
}

static GtkSvg *
svg_from_bytes (GBytes     *bytes,
                gboolean    is_symbolic,
                char      **unsupported)
{
  GtkSvg *svg;
  gulong handler_id;

  svg = gtk_svg_new ();

  if (is_symbolic)
    gtk_svg_set_features (svg, GTK_SVG_DEFAULT_FEATURES | GTK_SVG_TRADITIONAL_SYMBOLIC);

  handler_id = g_signal_connect (svg, "error", G_CALLBACK (svg_load_error), unsupported);
  gtk_svg_load_from_bytes (svg, bytes);
  g_signal_handler_disconnect (svg, handler_id);

  return svg;
}

/* }}} */
/* {{{ Symbolic processing */

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
                                     gboolean  *out_only_fg,
                                     GError   **error)

{
  GtkSvg *svg;
  const char *r_string = "rgb(255,0,0)";
  const char *g_string = "rgb(0,255,0)";
  gboolean only_fg;
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

      if (out_only_fg)
        *out_only_fg = TRUE;

      g_object_unref (svg);

      return texture;
    }

  only_fg = TRUE;
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

      only_fg &= extract_plane (loaded, data, width, height, 0, plane);

      g_object_unref (loaded);
    }

  texture = gdk_memory_texture_new (width, height,
                                    GDK_MEMORY_R8G8B8A8,
                                    data_bytes,
                                    4 * width);

out:
  g_bytes_unref (data_bytes);
  g_object_unref (svg);

  if (out_only_fg)
    *out_only_fg = only_fg;

  return texture;
}

/* }}} */
/* {{{ Texture-from-stream API */

static GBytes *
input_stream_get_bytes (GInputStream  *stream,
                        GError       **error)
{
  GOutputStream *out;
  gssize res;
  GBytes *bytes;

  out = g_memory_output_stream_new_resizable ();
  res = g_output_stream_splice (out, stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, NULL, error);

  if (res == -1)
    {
      g_object_unref (out);
      return NULL;
    }

  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (out));
  g_object_unref (out);

  return bytes;
}

GdkTexture *
gdk_texture_new_from_filename_at_scale (const char  *filename,
                                        int          width,
                                        int          height,
                                        GError     **error)
{
  GFile *file;
  GInputStream *stream;
  GBytes *bytes;
  GtkSvg *svg;
  GdkTexture *texture;
  char *unsupported = NULL;

  file = g_file_new_for_path (filename);
  stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
  g_object_unref (file);
  if (!stream)
    return NULL;

  bytes = input_stream_get_bytes (stream, error);
  g_object_unref (stream);
  if (!bytes)
    return NULL;

  svg = svg_from_bytes (bytes, FALSE, &unsupported);
  g_bytes_unref (bytes);
  if (unsupported)
    {
      g_clear_object (&svg);
      g_free (unsupported);
      return NULL;
    }

  texture = svg_to_texture (svg, width, height, NULL, 0);
  g_object_unref (svg);

  return texture;
}

/* }}} */
/* {{{ Paintable API */

static gboolean
is_symbolic (const char *path)
{
  return g_str_has_suffix (path, "-symbolic.svg") ||
         g_str_has_suffix (path, "-symbolic-ltr.svg") ||
         g_str_has_suffix (path, "-symbolic-rtl.svg");
}

static GdkPaintable *
gdk_paintable_new_from_bytes (GBytes     *bytes,
                              const char *path,
                              gboolean    is_symbolic)
{
  GdkPaintable *paintable;

  if (gdk_texture_can_load (bytes))
    paintable = GDK_PAINTABLE (gdk_texture_new_from_bytes (bytes, NULL));
  else
    {
      GtkSvg *svg;
      char *unsupported = NULL;

      svg = svg_from_bytes (bytes, is_symbolic, &unsupported);
      if (unsupported)
        {
          gboolean only_fg;

          if (is_symbolic)
            paintable = GDK_PAINTABLE (gdk_texture_new_from_bytes_symbolic (bytes, 0, 0, &only_fg, NULL));
          else
            paintable = GDK_PAINTABLE (gdk_texture_new_from_bytes (bytes, NULL));
          if (paintable)
            {
              GTK_DEBUG (ICONFALLBACK, "Falling back to a texture for %s: %s", path, unsupported);
              g_clear_object (&svg);
              g_free (unsupported);
            }
          else
            {
              GTK_DEBUG (ICONFALLBACK, "Expect misrendering; %s uses unsupported features: %s", path, unsupported);
              g_free (unsupported);
              paintable = GDK_PAINTABLE (svg);
            }
        }
      else
        paintable = GDK_PAINTABLE (svg);
    }

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_filename (const char  *filename,
                                 GError     **error)
{
  char *contents;
  gsize length;
  GBytes *bytes;
  GdkPaintable *paintable;

  if (!g_file_get_contents (filename, &contents, &length, error))
    return NULL;

  bytes = g_bytes_new_take (contents, length);
  paintable = gdk_paintable_new_from_bytes (bytes, filename, is_symbolic (filename));
  g_bytes_unref (bytes);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_resource (const char *path)
{
  GBytes *bytes;
  GdkPaintable *paintable;

  bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (!bytes)
    return NULL;

  paintable = gdk_paintable_new_from_bytes (bytes, path, is_symbolic (path));
  g_bytes_unref (bytes);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_file (GFile   *file,
                             GError **error)
{
  GBytes *bytes;
  GdkPaintable *paintable;
  char *uri;

  bytes = g_file_load_bytes (file, NULL, NULL, error);
  if (!bytes)
    return NULL;

  uri = g_file_get_uri (file);
  paintable = gdk_paintable_new_from_bytes (bytes, uri, is_symbolic (uri));
  g_bytes_unref (bytes);
  g_free (uri);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_stream (GInputStream  *stream,
                               GCancellable  *cancellable,
                               GError       **error)
{
  GBytes *bytes;
  GdkPaintable *paintable;

  bytes = input_stream_get_bytes (stream, error);
  if (!bytes)
    return NULL;

  paintable = gdk_paintable_new_from_bytes (bytes, "?", FALSE);
  g_bytes_unref (bytes);

  return paintable;
}

/* }}} */

/* vim:set foldmethod=marker: */

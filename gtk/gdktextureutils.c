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

#include "gdk/gdktextureprivate.h"
#include "gdk/loaders/gdkpngprivate.h"
#include "gdk/gdkdebugprivate.h"
#include "gtk/gtkdebug.h"
#include "gtk/gtkenums.h"
#include "gtk/gtkbitmaskprivate.h"

#include <librsvg/rsvg.h>

/* {{{ svg helpers */

static gboolean
gdk_texture_get_rsvg_handle_size (RsvgHandle *handle, gdouble *out_width, gdouble *out_height)
{
#if LIBRSVG_CHECK_VERSION (2,52,0)
  gboolean has_viewbox;
  RsvgRectangle viewbox;

  if (rsvg_handle_get_intrinsic_size_in_pixels (handle, out_width, out_height))
    return TRUE;

  rsvg_handle_get_intrinsic_dimensions (handle,
                                        NULL, NULL, NULL, NULL,
                                        &has_viewbox,
                                        &viewbox);

  if (has_viewbox)
    {
      *out_width = viewbox.width;
      *out_height = viewbox.height;
      return TRUE;
    }

  return FALSE;
#else
  RsvgDimensionData dim;
  rsvg_handle_get_dimensions (handle, &dim);
  if (out_width)
    *out_width = dim.width;
  if (out_height)
    *out_height = dim.height;
  return TRUE;
#endif
}

static GdkTexture *
gdk_texture_new_from_rsvg (RsvgHandle  *handle,
                           int          width,
                           int          height,
                           GError     **error)
{
  int stride;
  guchar *data;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkTexture *texture = NULL;

  stride = width * 4;
  data = g_new0 (guchar, stride * height);

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width, height,
                                                 stride);

  cr = cairo_create (surface);

#if !LIBRSVG_CHECK_VERSION (2,52,0)
  {
    RsvgDimensionData dim;
    gdouble sx,sy,s;

    rsvg_handle_get_dimensions (handle, &dim);
    sx = (gdouble)width / dim.width;
    sy = (gdouble)height / dim.height;
    s = MIN (sx, sy);

    cairo_scale (cr, s, s);
  }

  if (!rsvg_handle_render_cairo (handle, cr))
    g_set_error (error, GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                 "Error rendering SVG document (%s)", cairo_status_to_string (cairo_status (cr)));
  else
#else
  if (rsvg_handle_render_document (handle, cr,
                                   &(RsvgRectangle) { 0, 0, width, height },
                                   error))
#endif
    {
      GBytes *bytes;

      bytes = g_bytes_new (data, stride * height);
      texture = gdk_memory_texture_new (width, height,
                                        GDK_MEMORY_DEFAULT,
                                        bytes,
                                        stride);
      g_bytes_unref (bytes);
    }

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  g_free (data);

  return texture;
}

static GdkTexture *
gdk_texture_new_from_svg_bytes (GBytes  *bytes,
                                double   scale,
                                GError **error)
{
  const guchar *data;
  gsize len;
  RsvgHandle *handle;
  GdkTexture *texture;
  int width, height;
  double w, h;

  data = g_bytes_get_data (bytes, &len);
  handle = rsvg_handle_new_from_data (data, len, error);
  if (!handle)
    return NULL;

  if (!gdk_texture_get_rsvg_handle_size (handle, &w, &h))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Svg image has no intrinsic size; please set one");
      g_object_unref (handle);
      return NULL;
    }

  width = ceil (w * scale);
  height = ceil (h * scale);

  texture = gdk_texture_new_from_rsvg (handle, width, height, error);
  g_object_unref (handle);

  return texture;
}

/* }}} */
/* {{{ Symbolic processing */

static char *
make_stylesheet (const char *fg_color,
                 const char *success_color,
                 const char *warning_color,
                 const char *error_color)
{
  return g_strconcat ("rect,circle,path,.foreground-fill {\n"
                      " fill:", fg_color, "!important;\n"
                      "}\n"
                      ".warning,.warning-fill {\n"
                      " fill:", warning_color, "!important;\n"
                      "}\n"
                      ".error,.error-fill {\n"
                      "  fill:", error_color, "!important;\n"
                      "}\n"
                      ".success,.success-fill {\n"
                      "  fill:", success_color, "!important;\n"
                      "}\n"
                      ".transparent-fill {\n"
                      "  fill: none !important;\n"
                      "}\n"
                      ".foreground-stroke {\n"
                      "  stroke:", fg_color, "!important;\n"
                      "}\n"
                      ".warning-stroke {\n"
                      "  stroke:", warning_color, "!important;\n"
                      "}\n"
                      ".error-stroke {\n"
                      "  stroke:", error_color, "!important;\n"
                      "}\n"
                      ".success-stroke {\n"
                      "  stroke:", success_color, "!important;\n"
                      "}",
                      NULL);
}

static GdkTexture *
load_symbolic_svg (RsvgHandle  *handle,
                   int          width,
                   int          height,
                   const char  *fg_color,
                   const char  *success_color,
                   const char  *warning_color,
                   const char  *error_color,
                   GError     **error)
{
  GdkTexture *texture = NULL;
  char *stylesheet;

  stylesheet = make_stylesheet (fg_color, success_color, warning_color, error_color);

  if (!rsvg_handle_set_stylesheet (handle, (const guint8 *) stylesheet, strlen (stylesheet), error))
    {
      g_prefix_error (error, "Could not set stylesheet");
      goto out;
    }

  texture = gdk_texture_new_from_rsvg (handle, width, height, error);

out:
  g_free (stylesheet);

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
  RsvgHandle *handle;
  double w, h;
  const char *r_string = "rgb(255,0,0)";
  const char *g_string = "rgb(0,255,0)";
  gboolean only_fg;
  guchar *data;
  GBytes *data_bytes;
  GdkTexture *texture;

  handle = rsvg_handle_new_from_data (g_bytes_get_data (bytes, NULL),
                                      g_bytes_get_size (bytes),
                                      error);

  if (width == 0 || height == 0)
    {
      /* Fetch size from the original icon */
      if (!gdk_texture_get_rsvg_handle_size (handle, &w, &h))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Symbolic icon has no intrinsic size; please set one in its SVG");
          g_object_unref (handle);
          return NULL;
        }

      width = (int) ceil (w);
      height = (int) ceil (h);
    }

  if (!svg_has_symbolic_classes (bytes))
    {
      texture = gdk_texture_new_from_rsvg (handle, width, height, error);

      if (texture)
        texture = keep_alpha (texture);

      if (out_only_fg)
        *out_only_fg = TRUE;

      g_object_unref (handle);

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
      loaded = load_symbolic_svg (handle, width, height,
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
  g_object_unref (handle);

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
gdk_texture_new_from_stream_with_fg (GInputStream  *stream,
                                     gboolean      *only_fg,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  GBytes *bytes;
  GdkTexture *texture;

  bytes = input_stream_get_bytes (stream, error);
  if (!bytes)
    return NULL;

  texture = gdk_texture_new_from_bytes_with_fg (bytes, only_fg, error);

  g_bytes_unref (bytes);

  return texture;
}

/* Only called for svg */
GdkTexture *
gdk_texture_new_from_stream_at_scale (GInputStream  *stream,
                                      int            width,
                                      int            height,
                                      gboolean      *only_fg,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  RsvgHandle *handle;
  GdkTexture *texture;

  if (only_fg)
    *only_fg = FALSE;

  handle = rsvg_handle_new_from_stream_sync (stream, NULL, RSVG_HANDLE_FLAGS_NONE, NULL, error);
  if (!handle)
    return NULL;

  texture = gdk_texture_new_from_rsvg (handle, width, height, error);
  g_object_unref (handle);

  return texture;
}

GdkTexture *
gdk_texture_new_from_resource_at_scale (const char    *path,
                                        int            width,
                                        int            height,
                                        gboolean      *only_fg,
                                        GError       **error)
{
  GInputStream *stream;
  GdkTexture *texture;

  stream = g_resources_open_stream (path, 0, error);
  if (stream == NULL)
    return NULL;

  texture = gdk_texture_new_from_stream_at_scale (stream, width, height, only_fg, NULL, error);
  g_object_unref (stream);

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

static GdkPaintable *
gdk_paintable_new_from_bytes_scaled (GBytes *bytes,
                                     double  scale)
{
  if (gdk_texture_can_load (bytes))
    {
      /* We know these formats can't be scaled */
      return GDK_PAINTABLE (gdk_texture_new_from_bytes (bytes, NULL));
    }
  else if (scale == 1)
    {
      return GDK_PAINTABLE (gdk_texture_new_from_svg_bytes (bytes, scale, NULL));
    }
  else
    {
      GdkTexture *texture;
      GdkPaintable *paintable;

      texture = gdk_texture_new_from_svg_bytes (bytes, scale, NULL);
      paintable = gtk_scaler_new (GDK_PAINTABLE (texture), scale);
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
  guint n_paths;
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
set_missing_attribute_error (GError     **error,
                             const char  *name)
{
  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
               "Missing attribute: %s", name);
}

static gboolean
markup_filter_attributes (const char *element_name,
                          const char **attribute_names,
                          const char **attribute_values,
                          GError     **error,
                          const char  *name,
                          ...)
{
  va_list ap;
  guint64 filtered = 0;

  va_start (ap, name);
  while (name)
    {
      const char **ptr;

      ptr = va_arg (ap, const char **);

      if (ptr)
        *ptr = NULL;

      /* Ignoring a whole namespace */
      if (g_str_has_suffix (name, ":*"))
        {
          GPatternSpec *spec = g_pattern_spec_new (name);

          for (int i = 0; attribute_names[i]; i++)
            {
              if (filtered & (1ull << i))
                continue;

              if (g_pattern_spec_match_string (spec, attribute_names[i]))
                {
                  filtered |= 1ull << i;
                  continue;
                }
            }

          g_pattern_spec_free (spec);
        }

      for (int i = 0; attribute_names[i]; i++)
        {
          if (filtered & (1ull << i))
            continue;

          if (strcmp (attribute_names[i], name) == 0)
            {
              if (ptr)
                *ptr = attribute_values[i];
              filtered |= 1ull << i;
              break;
            }
        }

      name = va_arg (ap, const char *);
    }

  va_end (ap);

  for (guint i = 0; attribute_names[i]; i++)
    {
      if ((filtered & (1ull << i)) == 0)
        {
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                       "attribute '%s' is invalid for element '%s'",
                       attribute_names[i], element_name);
          return FALSE;
        }
    }

  return TRUE;
}

static GskPath *
circle_path_new (float cx,
                 float cy,
                 float radius)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (cx, cy), radius);
  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
rect_path_new (float x,
               float y,
               float width,
               float height,
               float rx,
               float ry)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  if (rx == 0 && ry == 0)
    gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (x, y, width, height));
  else
    gsk_path_builder_add_rounded_rect (builder,
                                       &(GskRoundedRect) { .bounds = GRAPHENE_RECT_INIT (x, y, width, height),
                                                           .corner = {
                                                             GRAPHENE_SIZE_INIT (rx, ry),
                                                             GRAPHENE_SIZE_INIT (rx, ry),
                                                             GRAPHENE_SIZE_INIT (rx, ry),
                                                             GRAPHENE_SIZE_INIT (rx, ry)
                                                           }
                                                         });
  return gsk_path_builder_free_to_path (builder);
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

  if (strcmp (element_name, "svg") == 0)
    {
      const char *width_attr = NULL;
      const char *height_attr = NULL;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                NULL,
                                "width", &width_attr,
                                "height", &height_attr,
                                NULL);

      if (width_attr == NULL)
        {
          set_missing_attribute_error (error, "width");
          return;
        }

      data->width = g_ascii_strtod (width_attr, &end);
      if (end && *end != '\0' && strcmp (end, "px") != 0)
        {
          set_attribute_error (error, "width", width_attr);
          return;
        }

      if (height_attr == NULL)
        {
          set_missing_attribute_error (error, "height");
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

      /* Done */
      return;
    }
  else if (strcmp (element_name, "g") == 0 ||
           strcmp (element_name, "defs") == 0 ||
           strcmp (element_name, "style") == 0 ||
           g_str_has_prefix (element_name, "sodipodi:") ||
           g_str_has_prefix (element_name, "inkscape:"))

    {
      /* Do nothing */
      return;
    }
  else if (strcmp (element_name, "circle") == 0)
    {
      const char *cx_attr = NULL;
      const char *cy_attr = NULL;
      const char *r_attr = NULL;
      float cx = 0;
      float cy = 0;
      float r = 0;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                NULL,
                                "cx", &cx_attr,
                                "cy", &cy_attr,
                                "r", &r_attr,
                                NULL);

      if (cx_attr)
        {
          cx = g_ascii_strtod (cx_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "cx", cx_attr);
              return;
            }
        }

      if (cy_attr)
        {
          cy = g_ascii_strtod (cy_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "cy", cy_attr);
              return;
            }
        }

      if (r_attr)
        {
          r = g_ascii_strtod (r_attr, &end);
          if ((end && *end != '\0') || r < 0)
            {
              set_attribute_error (error, "r", r_attr);
              return;
            }
        }

      if (r == 0)
        return;  /* nothing to do */

      path = circle_path_new (cx, cy, r);
    }
  else if (strcmp (element_name, "rect") == 0)
    {
      const char *x_attr = NULL;
      const char *y_attr = NULL;
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      const char *rx_attr = NULL;
      const char *ry_attr = NULL;
      float x = 0;
      float y = 0;
      float width = 0;
      float height = 0;
      float rx = 0;
      float ry = 0;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                NULL,
                                "x", &x_attr,
                                "y", &y_attr,
                                "width", &width_attr,
                                "height", &height_attr,
                                "rx", &rx_attr,
                                "ry", &ry_attr,
                                NULL);

      if (x_attr)
        {
          x = g_ascii_strtod (x_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "x", x_attr);
              return;
            }
        }

      if (y_attr)
        {
          y = g_ascii_strtod (y_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "y", y_attr);
              return;
            }
        }

      width = g_ascii_strtod (width_attr, &end);
      if ((end && *end != '\0') || width < 0)
        {
          set_attribute_error (error, "width", width_attr);
          return;
        }

      height = g_ascii_strtod (height_attr, &end);
      if ((end && *end != '\0') || height < 0)
        {
          set_attribute_error (error, "height", height_attr);
          return;
        }

      if (width == 0 || height == 0)
        return;  /* nothing to do */

      if (rx_attr)
        {
          rx = g_ascii_strtod (rx_attr, &end);
          if ((end && *end != '\0') || rx < 0)
            {
              set_attribute_error (error, "rx", rx_attr);
              return;
            }
        }

      if (ry_attr)
        {
          ry = g_ascii_strtod (ry_attr, &end);
          if ((end && *end != '\0') || ry < 0)
            {
              set_attribute_error (error, "ry", ry_attr);
              return;
            }
        }

      if (!rx_attr && ry_attr)
        rx = ry;
      else if (rx_attr && !ry_attr)
        ry = rx;

      path = rect_path_new (x, y, width, height, rx, ry);
    }
  else if (strcmp (element_name, "path") == 0)
    {
      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                NULL,
                                "d", &path_attr,
                                NULL);

      if (!path_attr)
        {
          set_missing_attribute_error (error, "d");
          return;
        }

      path = gsk_path_parse (path_attr);
      if (!path)
        {
          set_attribute_error (error, "d", path_attr);
          return;
        }
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Unhandled element: %s", element_name);
      return;
    }

  g_assert (path != NULL);

  if (!markup_filter_attributes (element_name,
                                 attribute_names,
                                 attribute_values,
                                 error,
                                 "class", &class_attr,
                                 "opacity", &opacity_attr,
                                 "fill-rule", &fill_rule_attr,
                                 "fill-opacity", &fill_opacity_attr,
                                 "stroke-width", &stroke_width_attr,
                                 "stroke-opacity", &stroke_opacity_attr,
                                 "stroke-linecap", &stroke_linecap_attr,
                                 "stroke-linejoin", &stroke_linejoin_attr,
                                 "stroke-miterlimit", &stroke_miterlimit_attr,
                                 "stroke-dasharray", &stroke_dasharray_attr,
                                 "stroke-dashoffset", &stroke_dashoffset_attr,
                                 "fill", NULL,
                                 "stroke", NULL,
                                 "style", NULL,
                                 "id", NULL,
                                 "color", NULL,
                                 "overflow", NULL,
                                 "d", NULL,
                                 "cx", NULL,
                                 "cy", NULL,
                                 "r", NULL,
                                 "gpa:*", NULL, /* Ignore gtk namespace attrs */
                                 NULL))
    goto cleanup;

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

  stroke = gsk_stroke_new (2);
  gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);
  gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_ROUND);

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
    {
      data->n_paths++;
      gtk_snapshot_append_fill (data->snapshot, path, fill_rule, &fill_color);
    }

  if (do_stroke)
    {
      data->n_paths++;
      gtk_snapshot_append_stroke (data->snapshot, path, stroke, &stroke_color);
    }

  if (opacity != 1)
    gtk_snapshot_pop (data->snapshot);

cleanup:
  g_clear_pointer (&path, gsk_path_unref);
  g_clear_pointer (&stroke, gsk_stroke_free);
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
gsk_render_node_new_from_bytes_symbolic (GBytes    *bytes,
                                         gboolean  *only_fg,
                                         gboolean  *single_path,
                                         double    *width,
                                         double    *height,
                                         GError   **error)
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
  const char *text;
  gsize len;

  data.width = data.height = 0;
  data.only_fg = TRUE;
  data.snapshot = gtk_snapshot_new ();
  data.has_clip = FALSE;
  data.n_paths = 0;

  text = g_bytes_get_data (bytes, &len);

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  if (!g_markup_parse_context_parse (context, text, len, error))
    {
      GskRenderNode *node;

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

  if (single_path)
    *single_path = data.n_paths == 1;

  *width = data.width;
  *height = data.height;

  return gtk_snapshot_free_to_node (data.snapshot);
}

GskRenderNode *
gsk_render_node_new_from_resource_symbolic (const char *path,
                                            gboolean   *only_fg,
                                            gboolean   *single_path,
                                            double     *width,
                                            double     *height)
{
  GBytes *bytes;
  GskRenderNode *node;
  GError *error = NULL;

  if (!gdk_has_feature (GDK_FEATURE_ICON_NODES))
    return NULL;

  bytes = g_resources_lookup_data (path, 0, NULL);
  if (!bytes)
    return NULL;

  node = gsk_render_node_new_from_bytes_symbolic (bytes, only_fg, single_path, width, height, &error);
  g_bytes_unref (bytes);
  if (error)
    {
      if (GTK_DEBUG_CHECK (ICONTHEME))
        gdk_debug_message ("Failed to convert resource %s to node: %s", path, error->message);
      g_error_free (error);
    }

  return node;
}

GskRenderNode *
gsk_render_node_new_from_filename_symbolic (const char *filename,
                                            gboolean   *only_fg,
                                            gboolean   *single_path,
                                            double     *width,
                                            double     *height)
{
  char *text;
  gsize len;
  GBytes *bytes;
  GskRenderNode *node;
  GError *error = NULL;

  if (!gdk_has_feature (GDK_FEATURE_ICON_NODES))
    return NULL;

  if (!g_file_get_contents (filename, &text, &len, NULL))
    return NULL;

  bytes = g_bytes_new_take (text, len);
  node = gsk_render_node_new_from_bytes_symbolic (bytes, only_fg, single_path, width, height, &error);
  g_bytes_unref (bytes);
  if (error)
    {
      if (GTK_DEBUG_CHECK (ICONTHEME))
        gdk_debug_message ("Failed to convert file %s to node: %s", filename, error->message);
      g_error_free (error);
    }

  return node;
}

/* }}} */
/* {{{ Render node recoloring */

static GskStroke *
apply_weight (const GskStroke *orig,
              float            weight)
{
  GskStroke *stroke = gsk_stroke_copy (orig);
  float min_width, width, max_width;

  width = gsk_stroke_get_line_width (orig);
  min_width = width * 0.25;
  max_width = width * 1.5;

  if (weight < 1.f)
    {
      g_assert_not_reached ();
    }
  else if (weight < 400.f)
    {
      float f = (400.f - weight) / (400.f - 1.f);
      width = min_width * f + width * (1.f - f);
    }
  else if (weight == 400.f)
    {
      /* nothing to do */
    }
  else if (weight <= 1000.f)
    {
      float f = (weight - 400.f) / (1000.f - 400.f);
      width = max_width * f + width * (1.f - f);
    }
  else
    {
      g_assert_not_reached ();
    }

  gsk_stroke_set_line_width (stroke, width);

  return stroke;
}

static gboolean
recolor_node (GskRenderNode *node,
              const GdkRGBA *colors,
              gsize          n_colors,
              float          weight,
              GtkSnapshot   *snapshot)
{
  switch ((int) gsk_render_node_get_node_type (node))
    {
    case GSK_CONTAINER_NODE:
      for (guint i = 0; i < gsk_container_node_get_n_children (node); i++)
        if (!recolor_node (gsk_container_node_get_child (node, i), colors, n_colors, weight, snapshot))
          return FALSE;
      return TRUE;

    case GSK_TRANSFORM_NODE:
      {
        gboolean ret;

        gtk_snapshot_save (snapshot);
        gtk_snapshot_transform (snapshot, gsk_transform_node_get_transform (node));
        ret = recolor_node (gsk_transform_node_get_child (node), colors, n_colors, weight, snapshot);
        gtk_snapshot_restore (snapshot);

        return ret;
      }

    case GSK_CLIP_NODE:
      {
        gboolean ret;

        gtk_snapshot_push_clip (snapshot, gsk_clip_node_get_clip (node));
        ret = recolor_node (gsk_clip_node_get_child (node), colors, n_colors, weight, snapshot);
        gtk_snapshot_pop (snapshot);

        return ret;
      }

    case GSK_OPACITY_NODE:
      {
        gboolean ret;

        gtk_snapshot_push_opacity (snapshot, gsk_opacity_node_get_opacity (node));
        ret = recolor_node (gsk_opacity_node_get_child (node), colors, n_colors, weight, snapshot);
        gtk_snapshot_pop (snapshot);

        return ret;
      }

    case GSK_FILL_NODE:
      {
        gboolean ret;

        gtk_snapshot_push_fill (snapshot,
                                gsk_fill_node_get_path (node),
                                gsk_fill_node_get_fill_rule (node));
        ret = recolor_node (gsk_fill_node_get_child (node), colors, n_colors, weight, snapshot);
        gtk_snapshot_pop (snapshot);

        return ret;
      }
      break;

    case GSK_STROKE_NODE:
      {
        gboolean ret;
        GskStroke *stroke;

        stroke = apply_weight (gsk_stroke_node_get_stroke (node), weight);
        gtk_snapshot_push_stroke (snapshot,
                                  gsk_stroke_node_get_path (node),
                                  stroke);
        gsk_stroke_free (stroke);

        ret = recolor_node (gsk_stroke_node_get_child (node), colors, n_colors, weight, snapshot);
        gtk_snapshot_pop (snapshot);

        return ret;
      }

    case GSK_COLOR_NODE:
      {
        graphene_rect_t bounds;
        GdkRGBA color;
        float alpha;

        gsk_render_node_get_bounds (node, &bounds);
        color = *gsk_color_node_get_color (node);

        /* Preserve the alpha that was set from fill-opacity */
        alpha = color.alpha;
        color.alpha = 1;

        if (gdk_rgba_equal (&color, &(GdkRGBA) { 0, 0, 0, 1 }))
          color = colors[GTK_SYMBOLIC_COLOR_FOREGROUND];
        else if (gdk_rgba_equal (&color, &(GdkRGBA) { 0, 0, 1, 1 }))
          color = colors[GTK_SYMBOLIC_COLOR_ERROR];
        else if (gdk_rgba_equal (&color, &(GdkRGBA) { 0, 1, 0, 1 }))
          color = colors[GTK_SYMBOLIC_COLOR_WARNING];
        else if (gdk_rgba_equal (&color, &(GdkRGBA) { 1, 0, 0, 1 }))
          color = colors[GTK_SYMBOLIC_COLOR_SUCCESS];

        color.alpha *= alpha;

        gtk_snapshot_append_color (snapshot, &color, &bounds);
      }
      return TRUE;

    default:
      return FALSE;
    }
}

gboolean
gsk_render_node_recolor (GskRenderNode  *node,
                         const GdkRGBA  *colors,
                         gsize           n_colors,
                         float           weight,
                         GskRenderNode **recolored)
{
  GtkSnapshot *snapshot;
  gboolean ret;

  if (gsk_render_node_get_node_type (node) == GSK_TEXTURE_NODE)
    {
      *recolored = NULL;
      return FALSE;
    }

  snapshot = gtk_snapshot_new ();
  ret = recolor_node (node, colors, n_colors, weight, snapshot);
  *recolored = gtk_snapshot_free_to_node (snapshot);

  if (!ret)
    g_clear_pointer (recolored, gsk_render_node_unref);

  return ret;
}

/* }}} */

/* vim:set foldmethod=marker: */

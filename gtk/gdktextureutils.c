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


#include <librsvg/rsvg.h>

/* {{{ svg helpers */

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

  if (rsvg_handle_render_document (handle, cr,
                                   &(RsvgRectangle) { 0, 0, width, height },
                                   error))
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

  if (!rsvg_handle_get_intrinsic_size_in_pixels (handle, &w, &h))
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
  return g_strconcat ("    rect,circle,path {\n"
                      "      fill: ", fg_color," !important;\n"
                      "    }\n"
                      "    .warning {\n"
                      "      fill: ", warning_color, " !important;\n"
                      "    }\n"
                      "    .error {\n"
                      "      fill: ", error_color," !important;\n"
                      "    }\n"
                      "    .success {\n"
                      "      fill: ", success_color, " !important;\n"
                      "    }\n",
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

  return memmem (data, len, "class=\"error\"", strlen ("class=\"error\"")) != NULL ||
         memmem (data, len, "class=\"warning\"", strlen ("class=\"warning\"")) != NULL ||
         memmem (data, len, "class=\"success\"", strlen ("class=\"success\"")) != NULL;
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
      if (!rsvg_handle_get_intrinsic_size_in_pixels (handle, &w, &h))
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

      return texture;
    }

  only_fg = TRUE;
  texture = NULL;
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
  else
    {
      return GDK_PAINTABLE (gdk_texture_new_from_svg_bytes (bytes, scale, NULL));
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
start_element_cb (GMarkupParseContext *context,
                  const gchar         *element_name,
                  const gchar        **attribute_names,
                  const gchar        **attribute_values,
                  gpointer             user_data,
                  GError             **error)
{
  ParserData *data = user_data;

  if (strcmp (element_name, "svg") == 0)
    {
      const char *width = NULL;
      const char *height = NULL;
      char *end;

      for (int i = 0; attribute_names[i]; i++)
        {
          if (strcmp (attribute_names[i], "width") == 0)
            width = attribute_values[i];
          else if (strcmp (attribute_names[i], "height") == 0)
            height = attribute_values[i];
        }

      if (!width)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                       "Missing attribute: %s", width);
          return;
        }
      data->width = g_ascii_strtod (width, &end);
      if (end && *end != '\0' && strcmp (end, "px") != 0)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid width attribute: %s", width);
          return;
        }

      if (!height)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                       "Missing attribute: %s", height);
          return;
        }

      data->height = g_ascii_strtod (height, &end);
      if (end && *end != '\0' && strcmp (end, "px") != 0)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid height attribute: %s", height);
          return;
        }

      gtk_snapshot_push_clip (data->snapshot, &GRAPHENE_RECT_INIT (0, 0, data->width, data->height));
      data->has_clip = TRUE;
      gtk_snapshot_append_color (data->snapshot,
                                 &(GdkRGBA) { 0, 0, 0, 0 },
                                 &GRAPHENE_RECT_INIT (0, 0, data->width, data->height));
    }
  else if (strcmp (element_name, "g") == 0)
    {
      /* Do nothing */
    }
  else if (strcmp (element_name, "p") == 0 ||
           strcmp (element_name, "path") == 0)
    {
      const char *path_attr = NULL;
      const char *fill_rule_attr = NULL;
      const char *fill_opacity_attr = NULL;
      const char *opacity_attr = NULL;
      const char *class_attr = NULL;
      const char *style_attr = NULL;
      GskPath *path;
      GskFillRule fill_rule;
      double opacity;
      GdkRGBA color;
      char *end;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "d", &path_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill-rule", &fill_rule_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill-opacity", &fill_opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "opacity", &opacity_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "class", &class_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "fill", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "style", &style_attr,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "id", NULL,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      if (fill_rule_attr && strcmp (fill_rule_attr, "evenodd") == 0)
        fill_rule = GSK_FILL_RULE_EVEN_ODD;
      else
        fill_rule = GSK_FILL_RULE_WINDING;

      opacity = 1;
      if (fill_opacity_attr)
        {
          opacity = g_ascii_strtod (fill_opacity_attr, &end);
          if (end && *end != '\0')
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid fill-opacity attribute: %s", fill_opacity_attr);
              return;
            }
        }
      else if (opacity_attr)
        {
          opacity = g_ascii_strtod (opacity_attr, &end);
          if (end && *end != '\0')
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid opacity attribute: %s", fill_opacity_attr);
              return;
            }
        }
      else if (style_attr)
        {
          char *p;

          p = strstr (style_attr, "opacity:");
          if (p)
            {
              opacity = g_ascii_strtod (p, &end);
              if (end && *end != '\0' && *end != ';')
                {
                  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                               "Failed to parse opacity in style attribute: %s", style_attr);
                  return;
                }
            }
        }

      if (class_attr)
        data->only_fg = FALSE;
      else
        class_attr = "foreground";

      if (strcmp (class_attr, "foreground") == 0)
        color = (GdkRGBA) { 0, 0, 0, opacity };
      else if (strcmp (class_attr, "success") == 0)
        color = (GdkRGBA) { 1, 0, 0, opacity };
      else if (strcmp (class_attr, "warning") == 0)
        color = (GdkRGBA) { 0, 1, 0, opacity };
      else if (strcmp (class_attr, "error") == 0)
        color = (GdkRGBA) { 0, 0, 1, opacity };
      else
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Unsupported class: %s", class_attr);
          return;
        }

      path = gsk_path_parse (path_attr);
      if (!path)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Failed to parse path: %s", path_attr);
          return;
        }

      gtk_snapshot_append_fill (data->snapshot, path, fill_rule, &color);

      gsk_path_unref (path);
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Unhandled element: %s", element_name);
      return;
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
                                         gboolean *only_fg)
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

  return gtk_snapshot_free_to_node (data.snapshot);
}

GskRenderNode *
gsk_render_node_new_from_resource_symbolic (const char *path,
                                            gboolean   *only_fg)
{
  GBytes *bytes;
  GskRenderNode *node;

  if (!gdk_has_feature (GDK_FEATURE_ICON_NODES))
    return NULL;

  bytes = g_resources_lookup_data (path, 0, NULL);
  if (!bytes)
    return NULL;

  node = gsk_render_node_new_from_bytes_symbolic (bytes, only_fg);
  g_bytes_unref (bytes);

  return node;
}

GskRenderNode *
gsk_render_node_new_from_filename_symbolic (const char *filename,
                                            gboolean   *only_fg)
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
  node = gsk_render_node_new_from_bytes_symbolic (bytes, only_fg);
  g_bytes_unref (bytes);

  return node;
}

/* }}} */

/* vim:set foldmethod=marker: */

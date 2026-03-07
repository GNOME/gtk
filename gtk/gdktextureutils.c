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
  struct {
    const char *location;
    gboolean unsupported;
  } *data = user_data;

  if (g_error_matches (error, GTK_SVG_ERROR, GTK_SVG_ERROR_NOT_IMPLEMENTED))
    {
      if (!data->unsupported)
        GTK_DEBUG (ICONFALLBACK, "Falling back to a texture for %s: %s", data->location, error->message);
      data->unsupported = TRUE;
    }
}

static GtkSvg *
svg_from_bytes (GBytes     *bytes,
                const char *path,
                gboolean    is_symbolic)
{
  GtkSvg *svg;
  gulong handler_id;
  struct {
    const char *location;
    gboolean unsupported;
  } data;

  svg = gtk_svg_new ();

  if (is_symbolic)
    gtk_svg_set_features (svg, GTK_SVG_DEFAULT_FEATURES | GTK_SVG_TRADITIONAL_SYMBOLIC);

  data.location = path;
  data.unsupported = FALSE;

  handler_id = g_signal_connect (svg, "error", G_CALLBACK (svg_load_error), &data);
  gtk_svg_load_from_bytes (svg, bytes);
  g_signal_handler_disconnect (svg, handler_id);

  if (data.unsupported)
    g_clear_object (&svg);

  return svg;
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

  file = g_file_new_for_path (filename);
  stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
  g_object_unref (file);
  if (!stream)
    return NULL;

  bytes = input_stream_get_bytes (stream, error);
  g_object_unref (stream);
  if (!bytes)
    return NULL;

  svg = svg_from_bytes (bytes, filename, FALSE);
  g_bytes_unref (bytes);
  if (!svg)
    return NULL;

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
      paintable = GDK_PAINTABLE (svg_from_bytes (bytes, path, is_symbolic));
      if (!paintable)
        {
          paintable = GDK_PAINTABLE (gdk_texture_new_from_bytes (bytes, NULL));
        }
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
  const char *path;

  bytes = g_file_load_bytes (file, NULL, NULL, error);
  if (!bytes)
    return NULL;

  path = g_file_peek_path (file);
  paintable = gdk_paintable_new_from_bytes (bytes, path, is_symbolic (path));
  g_bytes_unref (bytes);

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

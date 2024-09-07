/*  Copyright 2024 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"
#include <gtk.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtk-image-tool.h"


#define ALIGN(x,y) (((x + y - 1) / y) * y)

static guint
gdk_format_get_bpp (GdkMemoryFormat format)
{
  guint bpp[] = {
    4, 4, 4, 4, 4, 4, 4,
    3, 3,
    6, 8, 8,
    6, 8, 8,
    12, 16, 16,
    2, 2, 1,
    4, 4, 2, 1,
    2, 2,
    4,
    4, 4, 4, 4,
    0
  };

  g_assert (format < G_N_ELEMENTS (bpp));

  return bpp[format];
}

gboolean
gdk_texture_dump (GdkTexture  *texture,
                  const char  *filename,
                  GError     **error)
{
  GString *header;
  GdkColorState *cs;
  GdkCicpParams *params;
  GdkTextureDownloader *downloader;
  GdkMemoryFormat format;
  gsize width, height;
  gsize stride;
  guint bpp;
  char buf[20];
  guchar *data;
  gboolean res;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  format = gdk_texture_get_format (texture);

  bpp = gdk_format_get_bpp (format);
  stride = ALIGN (width * bpp, 8);

  cs = gdk_texture_get_color_state (texture);
  params = gdk_color_state_create_cicp_params (cs);

  header = g_string_new ("GTK texture dump\n\n");

  g_string_append_printf (header, "offset XXXX\n");
  g_string_append_printf (header, "stride %lu\n", stride);
  g_string_append_printf (header, "size %lu %lu\n", width, height);
  g_string_append_printf (header, "format %u\n", format);
  g_string_append_printf (header, "cicp %u/%u/%u/%u\n\n",
                          gdk_cicp_params_get_color_primaries (params),
                          gdk_cicp_params_get_transfer_function (params),
                          gdk_cicp_params_get_matrix_coefficients (params),
                          gdk_cicp_params_get_range (params));

  g_snprintf (buf, sizeof (buf), "%4lu", header->len);
  g_string_replace (header, "XXXX", buf, 1);

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, format);
  gdk_texture_downloader_set_color_state (downloader, cs);

  data = g_malloc (header->len + height * stride);

  memcpy (data, header->str, header->len);

  gdk_texture_downloader_download_into (downloader, data + header->len, stride);

  res = g_file_set_contents (filename, (char *) data, header->len + height * stride, error);

  gdk_texture_downloader_free (downloader);
  g_object_unref (params);
  gdk_color_state_unref (cs);
  g_free (data);

  return res;
}

GdkTexture *
gdk_texture_undump (const char  *filename,
                    GError     **error)
{
  char *data;
  guint parts;
  gsize len;
  gsize offset, stride, width, height;
  GdkMemoryFormat format;
  guint cp, tf, mc, range;
  GBytes *bytes;
  GdkCicpParams *params;
  GdkColorState *cs;
  GdkMemoryTextureBuilder *builder;
  guint bpp;
  GdkTexture *texture;

  if (!g_file_get_contents (filename, &data, &len, error))
    return NULL;

  parts = sscanf (data,
                  "GTK texture dump\n\n"
                  "offset %lu\n"
                  "stride %lu\n"
                  "size %lu %lu\n"
                  "format %u\n"
                  "cicp %u/%u/%u/%u\n\n",
                  &offset,
                  &stride,
                  &width, &height,
                  &format,
                  &cp, &tf, &mc, &range);

  if (parts < 9)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to parse header (after %d parts)", parts);
      g_free (data);
      return NULL;
    }

  if (format >= GDK_MEMORY_N_FORMATS)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid memory format (%u)", format);
      g_free (data);
      return NULL;
    }

  if (width == 0 || height == 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid size (%lu x %lu)", width, height);
      g_free (data);
      return NULL;
    }

  bpp = gdk_format_get_bpp (format);

  if (stride < bpp * width)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid stride (%lu < %u * %lu)", stride, bpp, width);
      g_free (data);
      return NULL;
    }

  if (len != offset + stride * height)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid size (%lu !=  %lu + %lu * %lu)", len, offset, stride, height);
      g_free (data);
      return NULL;
    }

  params = gdk_cicp_params_new ();
  gdk_cicp_params_set_color_primaries (params, cp);
  gdk_cicp_params_set_transfer_function (params, tf);
  gdk_cicp_params_set_matrix_coefficients (params, mc);
  gdk_cicp_params_set_range (params, (GdkCicpRange) range);
  cs = gdk_cicp_params_build_color_state (params, error);
  g_object_unref (params);

  if (cs == NULL)
    {
      g_free (data);
      return NULL;
    }

  bytes = g_bytes_new_with_free_func (data + offset, stride * height, g_free, data);

  builder = gdk_memory_texture_builder_new ();
  gdk_memory_texture_builder_set_bytes (builder, bytes);
  gdk_memory_texture_builder_set_stride (builder, stride);
  gdk_memory_texture_builder_set_width (builder, width);
  gdk_memory_texture_builder_set_height (builder, height);
  gdk_memory_texture_builder_set_format (builder, format);
  gdk_memory_texture_builder_set_color_state (builder, cs);

  texture = gdk_memory_texture_builder_build (builder);

  g_object_unref (builder);
  g_bytes_unref (bytes);
  gdk_color_state_unref (cs);

  return texture;
}

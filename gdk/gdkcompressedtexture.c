/*
 * Copyright © 2021 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdkcompressedtextureprivate.h"

#include "loaders/gdkpngprivate.h"
#include "loaders/gdktiffprivate.h"
#include "loaders/gdkjpegprivate.h"

struct _GdkCompressedTexture
{
  GdkTexture parent_instance;

  GBytes *bytes;
  GdkTexture *texture;
  guint remove_id;
};

struct _GdkCompressedTextureClass
{
  GdkTextureClass parent_class;
};

G_DEFINE_TYPE (GdkCompressedTexture, gdk_compressed_texture, GDK_TYPE_TEXTURE)

static gboolean
gdk_compressed_texture_clear (gpointer data)
{
  GdkCompressedTexture *self = data;

  g_print ("CLEAR %zu %u\n", g_bytes_get_size (self->bytes), self->texture ? G_OBJECT (self->texture)->ref_count : 0);
  g_clear_object (&self->texture);
  g_clear_handle_id (&self->remove_id, g_source_remove);

  return FALSE;
}

static GdkTexture *
gdk_compresed_texture_load (GBytes  *bytes,
                            GError **error)
{
  const char *data;
  gsize size;

  data = g_bytes_get_data (bytes, &size);

  if (size > strlen (PNG_SIGNATURE) &&
      memcmp (data, PNG_SIGNATURE, strlen (PNG_SIGNATURE)) == 0)
    {
      return gdk_load_png (bytes, error);
    }
  else if ((size > strlen (TIFF_SIGNATURE1) &&
            memcmp (data, TIFF_SIGNATURE1, strlen (TIFF_SIGNATURE1)) == 0) ||
           (size > strlen (TIFF_SIGNATURE2) &&
            memcmp (data, TIFF_SIGNATURE2, strlen (TIFF_SIGNATURE2)) == 0))
    {
      return gdk_load_tiff (bytes, error);
    }
  else if (size > strlen (JPEG_SIGNATURE) &&
           memcmp (data, JPEG_SIGNATURE, strlen (JPEG_SIGNATURE)) == 0)
    {
      return gdk_load_jpeg (bytes, error);
    }
  else
    {
      GInputStream *stream;
      GdkPixbuf *pixbuf;
      GdkTexture *texture;

      stream = g_memory_input_stream_new_from_bytes (bytes);
      pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, error);
      g_object_unref (stream);
      if (pixbuf == NULL)
        return NULL;

      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
      return texture;
    }
}

static void
gdk_compressed_texture_ensure_texture (GdkCompressedTexture *self)
{
  g_print ("%9zu %p Ensuring texture\n", g_bytes_get_size (self->bytes), self);
  if (self->texture == NULL)
    {
      self->texture = gdk_compresed_texture_load (self->bytes, NULL);
      g_assert (self->texture);
    }
  else
    {
      g_clear_handle_id (&self->remove_id, g_source_remove);
    }

  self->remove_id = g_timeout_add_seconds (10, gdk_compressed_texture_clear, self);
}

static void
gdk_compressed_texture_dispose (GObject *object)
{
  GdkCompressedTexture *self = GDK_COMPRESSED_TEXTURE (object);

  gdk_compressed_texture_clear (self);
  g_clear_pointer (&self->bytes, g_bytes_unref);

  G_OBJECT_CLASS (gdk_compressed_texture_parent_class)->dispose (object);
}

static GdkTexture *
gdk_compressed_texture_download_texture (GdkTexture *texture)
{
  GdkCompressedTexture *self = GDK_COMPRESSED_TEXTURE (texture);

  gdk_compressed_texture_ensure_texture (self);

  return g_object_ref (self->texture);
}

static void
gdk_compressed_texture_class_init (GdkCompressedTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download_texture = gdk_compressed_texture_download_texture;
  gobject_class->dispose = gdk_compressed_texture_dispose;
}

static void
gdk_compressed_texture_init (GdkCompressedTexture *self)
{
}

GdkTexture *
gdk_compressed_texture_new_from_bytes (GBytes  *bytes,
                                       GError **error)
{
  GdkCompressedTexture *self;
  GdkTexture *texture;

  texture = gdk_compresed_texture_load (bytes, error);
  if (texture == NULL)
    return NULL;

  self = g_object_new (GDK_TYPE_COMPRESSED_TEXTURE,
                       "width", gdk_texture_get_width (texture),
                       "height", gdk_texture_get_height (texture),
                       NULL);

  self->bytes = g_bytes_ref (bytes);
  self->texture = texture;
  /* install the removal handler */
  gdk_compressed_texture_ensure_texture (self);

  return GDK_TEXTURE (self);
}


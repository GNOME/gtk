/* GTK - The GIMP Toolkit
 * Copyright (C) 2023 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GdkTextureDownloader:
 *
 * Used to download the contents of a [class@Gdk.Texture].
 *
 * It is intended to be created as a short-term object for a single download,
 * but can be used for multiple downloads of different textures or with different
 * settings.
 *
 * `GdkTextureDownloader` can be used to convert data between different formats.
 * Create a `GdkTexture` for the existing format and then download it in a
 * different format.
 *
 * Since: 4.10
 */

#include "config.h"

#include "gdktexturedownloaderprivate.h"

#include "gdkcolorstateprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdktextureprivate.h"

G_DEFINE_BOXED_TYPE (GdkTextureDownloader, gdk_texture_downloader,
                     gdk_texture_downloader_copy,
                     gdk_texture_downloader_free)


void
gdk_texture_downloader_init (GdkTextureDownloader *self,
                             GdkTexture           *texture)
{
  self->texture = g_object_ref (texture);
  self->format = GDK_MEMORY_DEFAULT;
  self->color_state = gdk_color_state_ref (GDK_COLOR_STATE_SRGB);
}

void
gdk_texture_downloader_finish (GdkTextureDownloader *self)
{
  g_object_unref (self->texture);
  gdk_color_state_unref (self->color_state);
}

/**
 * gdk_texture_downloader_new:
 * @texture: texture to download
 *
 * Creates a new texture downloader for @texture.
 *
 * By default, the downloader will convert the data to
 * the default memory format, and to the sRGB color state.
 *
 * Returns: A new texture downloader
 *
 * Since: 4.10
 **/
GdkTextureDownloader *
gdk_texture_downloader_new (GdkTexture *texture)
{
  GdkTextureDownloader *self;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);

  self = g_new (GdkTextureDownloader, 1);
  gdk_texture_downloader_init (self, texture);

  return self;
}

/**
 * gdk_texture_downloader_copy:
 * @self: the downloader to copy
 *
 * Creates a copy of the downloader.
 *
 * This function is meant for language bindings.
 *
 * Returns: A copy of the downloader
 *
 * Since: 4.10
 **/
GdkTextureDownloader *
gdk_texture_downloader_copy (const GdkTextureDownloader *self)
{
  GdkTextureDownloader *copy;

  g_return_val_if_fail (self != NULL, NULL);

  copy = gdk_texture_downloader_new (self->texture);
  gdk_texture_downloader_set_format (copy, self->format);

  return copy;
}

/**
 * gdk_texture_downloader_free:
 * @self: texture downloader to free
 *
 * Frees the given downloader and all its associated resources.
 *
 * Since: 4.10
 **/
void
gdk_texture_downloader_free (GdkTextureDownloader *self)
{
  g_return_if_fail (self != NULL);

  gdk_texture_downloader_finish (self);
  g_free (self);
}

/**
 * gdk_texture_downloader_set_texture:
 * @self: a texture downloader
 * @texture: the new texture to download
 *
 * Changes the texture the downloader will download.
 *
 * Since: 4.10
 **/
void
gdk_texture_downloader_set_texture (GdkTextureDownloader *self,
                                    GdkTexture           *texture)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (GDK_IS_TEXTURE (texture));

  g_set_object (&self->texture, texture);
}

/**
 * gdk_texture_downloader_get_texture:
 * @self: a texture downloader
 *
 * Gets the texture that the downloader will download.
 *
 * Returns: (transfer none): The texture to download
 *
 * Since: 4.10
 **/
GdkTexture *
gdk_texture_downloader_get_texture (const GdkTextureDownloader *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->texture;
}

/**
 * gdk_texture_downloader_set_format:
 * @self: a texture downloader
 * @format: the format to use
 *
 * Sets the format the downloader will download.
 *
 * By default, GDK_MEMORY_DEFAULT is set.
 *
 * Since: 4.10
 */
void
gdk_texture_downloader_set_format (GdkTextureDownloader *self,
                                   GdkMemoryFormat       format)
{
  g_return_if_fail (self != NULL);

  self->format = format;
}

/**
 * gdk_texture_downloader_get_format:
 * @self: a texture downloader
 *
 * Gets the format that the data will be downloaded in.
 *
 * Returns: The format of the download
 *
 * Since: 4.10
 **/
GdkMemoryFormat
gdk_texture_downloader_get_format (const GdkTextureDownloader *self)
{
  g_return_val_if_fail (self != NULL, GDK_MEMORY_DEFAULT);

  return self->format;
}

/**
 * gdk_texture_downloader_set_color_state:
 * @self: a texture downloader
 * @color_state: the color state to use
 *
 * Sets the color state the downloader will convert the data to.
 *
 * By default, the sRGB colorstate returned by [func@ColorState.get_srgb]
 * is used.
 *
 * Since: 4.16
 */
void
gdk_texture_downloader_set_color_state (GdkTextureDownloader *self,
                                        GdkColorState        *color_state)
{
  if (self->color_state == color_state)
    return;

  gdk_color_state_unref (self->color_state);
  self->color_state = gdk_color_state_ref (color_state);
}

/**
 * gdk_texture_downloader_get_color_state:
 * @self: a texture downloader
 *
 * Gets the color state that the data will be downloaded in.
 *
 * Returns: The color state of the download
 *
 * Since: 4.16
 **/
GdkColorState *
gdk_texture_downloader_get_color_state (const GdkTextureDownloader *self)
{
  return self->color_state;
}

void
gdk_texture_downloader_download_into_layout (const GdkTextureDownloader *self,
                                             guchar                     *data,
                                             const GdkMemoryLayout      *layout)
{
  gdk_texture_do_download (self->texture,
                           data,
                           layout,
                           self->color_state);
}

/**
 * gdk_texture_downloader_download_into:
 * @self: a texture downloader
 * @data: (array): pointer to enough memory to be filled with the
 *   downloaded data of the texture
 * @stride: rowstride in bytes
 *
 * Downloads the @texture into local memory.
 *
 * This function cannot be used with a multiplanar format.
 *
 * Since: 4.10
 **/
void
gdk_texture_downloader_download_into (const GdkTextureDownloader *self,
                                      guchar                     *data,
                                      gsize                       stride)
{
  GdkMemoryLayout layout;
  g_return_if_fail (self != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (gdk_memory_format_get_n_planes (self->format) == 1);
  layout = GDK_MEMORY_LAYOUT_SIMPLE (self->format,
                                     self->texture->width,
                                     self->texture->height,
                                     stride);
  gdk_memory_layout_return_if_invalid (&layout);

  gdk_texture_downloader_download_into_layout (self, data, &layout);
}

GBytes *
gdk_texture_downloader_download_bytes_layout (const GdkTextureDownloader *self,
                                              GdkMemoryLayout            *out_layout)
{
  if (gdk_texture_get_format (self->texture) == self->format &&
      gdk_color_state_equal (gdk_texture_get_color_state (self->texture), self->color_state))
    {
      return gdk_texture_download_bytes (self->texture, out_layout);
    }
  else
    {
      guchar *data;

      gdk_memory_layout_init (out_layout,
                              self->format,
                              self->texture->width,
                              self->texture->height,
                              1);
      data = g_malloc (out_layout->size);
      
      gdk_texture_do_download (self->texture, data, out_layout, self->color_state);
      return g_bytes_new_take (data, out_layout->size);
    }
}

/**
 * gdk_texture_downloader_download_bytes:
 * @self: the downloader
 * @out_stride: (out): The stride of the resulting data in bytes
 *
 * Downloads the given texture pixels into a `GBytes`. The rowstride will
 * be stored in the stride value.
 *
 * This function will abort if it tries to download a large texture and
 * fails to allocate memory. If you think that may happen, you should handle
 * memory allocation yourself and use [method@Gdk.TextureDownloader.download_into]
 * once allocation succeeded.
 *
 * This function cannot be used with a multiplanar format. Use
 * [method@Gdk.TextureDownloader.download_bytes_with_planes] for that purpose.
 *
 * Returns: The downloaded pixels
 *
 * Since: 4.10
 **/
GBytes *
gdk_texture_downloader_download_bytes (const GdkTextureDownloader *self,
                                       gsize                      *out_stride)
{
  GBytes *bytes;
  GdkMemoryLayout layout;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (out_stride != NULL, NULL);
  g_return_val_if_fail (gdk_memory_format_get_n_planes (self->format) == 1, NULL);

  bytes = gdk_texture_downloader_download_bytes_layout (self, &layout);

  if (layout.planes[0].offset)
    {
      GBytes *tmp = g_bytes_new_from_bytes (bytes,
                                            layout.planes[0].offset,
                                            g_bytes_get_size (bytes) - layout.planes[0].offset);
      g_bytes_unref (bytes);
      bytes = tmp;
    }

  *out_stride = layout.planes[0].stride;
  return bytes;
}

/**
 * gdk_texture_downloader_download_bytes_with_planes:
 * @self: the downloader
 * @out_offsets: (out caller-allocates) (array fixed-size=4):
 *   The offsets of the resulting data planes in bytes
 * @out_strides: (out caller-allocates) (array fixed-size=4):
 *   The stride of the resulting data planes in bytes
 *
 * Downloads the given texture pixels into a `GBytes`. The offsets and
 * strides of the resulting buffer will be stored in the respective values.
 *
 * If the format does have less than 4 planes, the remaining offsets and strides will be
 * set to `0`.
 *
 * Returns: The downloaded pixels
 *
 * Since: 4.20
 **/
GBytes *
gdk_texture_downloader_download_bytes_with_planes (const GdkTextureDownloader *self,
                                                   gsize                       out_offsets[4],
                                                   gsize                       out_strides[4])
{
  GBytes *bytes;
  GdkMemoryLayout layout;
  gsize p;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (out_offsets != NULL, NULL);
  g_return_val_if_fail (out_strides != NULL, NULL);

  bytes = gdk_texture_downloader_download_bytes_layout (self, &layout);

  for (p = 0; p < gdk_memory_format_get_n_planes (layout.format); p++)
    {
      out_offsets[p] = layout.planes[p].offset;
      out_strides[p] = layout.planes[p].stride;
    }
  for (; p < 4; p++)
    {
      out_offsets[p] = 0;
      out_strides[p] = 0;
    }

  return bytes;
}


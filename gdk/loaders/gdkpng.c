/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 Red Hat, Inc.
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

#include "gdkpngprivate.h"

#include "gdktexture.h"
#include "gdktextureprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gsk/ngl/fp16private.h"
#include <png.h>
#include <stdio.h>

/* The main difference between the png load/save code here and
 * gdk-pixbuf is that we can support loading 16-bit data in the
 * future, and we can extract gamma and colorspace information
 * to produce linear, color-corrected data.
 */

/* {{{ Format conversion */

static void
flip_02 (guchar *data,
         int     width,
         int     height,
         int     stride)
{
  gsize x, y;

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          guchar tmp;
          tmp = data[x * 4];
          data[x * 4] = data[x * 4 + 2];
          data[x * 4 + 2] = tmp;
        }
      data += stride;
    }
}

static void
convert_float_to_16bit_inplace (float   *src,
                                int      width,
                                int      height)
{
  gsize x, y;
  guint16 *dest = (guint16 *)src;

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          dest[4 * x    ] = (guint16) CLAMP(65536.f * src[x * 4    ], 0.f, 65535.f);
          dest[4 * x + 1] = (guint16) CLAMP(65536.f * src[x * 4 + 1], 0.f, 65535.f);
          dest[4 * x + 2] = (guint16) CLAMP(65536.f * src[x * 4 + 2], 0.f, 65535.f);
          dest[4 * x + 3] = (guint16) CLAMP(65536.f * src[x * 4 + 3], 0.f, 65535.f);
        }
      dest += width * 4;
      src += width * 4;
    }
} 

/* }}} */
/* {{{ Public API */

GdkTexture *
gdk_load_png (GBytes  *bytes,
              GError **error)
{
  png_image image = { NULL, PNG_IMAGE_VERSION, 0, };
  gsize size;
  gsize stride;
  guchar *buffer;
  GdkTexture *texture;
  GBytes *out_bytes;

  png_image_begin_read_from_memory (&image,
                                    g_bytes_get_data (bytes, NULL),
                                    g_bytes_get_size (bytes));

  if (PNG_IMAGE_FAILED (image))
    {
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED,
                   "Failed to parse png image (%s)", image.message);
      png_image_free (&image);
      return NULL;
    }

  image.format = PNG_FORMAT_RGBA;

  stride = PNG_IMAGE_ROW_STRIDE (image);
  size = PNG_IMAGE_BUFFER_SIZE (image, stride);
  buffer = g_try_malloc (size);
  if (!buffer)
    {
      g_set_error_literal (error,
                           GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_INSUFFICIENT_MEMORY,
                           "Not enough memory to load png");
      png_image_free (&image);
      return NULL;
    }

  png_image_finish_read (&image, NULL, buffer, stride, NULL);

  if (PNG_IMAGE_FAILED (image))
    {
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED,
                   "Failed to parse png image (%s)", image.message);
      png_image_free (&image);
      return NULL;
    }

  if (image.format & PNG_FORMAT_FLAG_LINEAR)
    stride *= 2;

  out_bytes = g_bytes_new_take (buffer, size);

  texture = gdk_memory_texture_new (image.width, image.height,
                                    GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                                    out_bytes, stride);

  g_bytes_unref (out_bytes);

  png_image_free (&image);

  return texture;
}

GBytes *
gdk_save_png (GdkTexture *texture)
{
  png_image image = { NULL, PNG_IMAGE_VERSION, 0, };
  int stride;
  const guchar *data;
  guchar *new_data = NULL;
  png_alloc_size_t size;
  gpointer buffer;
  GdkTexture *memory_texture;
  GdkMemoryFormat format;
  gboolean result G_GNUC_UNUSED;

  image.width = gdk_texture_get_width (texture);
  image.height = gdk_texture_get_height (texture);

  memory_texture = gdk_texture_download_texture (texture);
  format = gdk_memory_texture_get_format (GDK_MEMORY_TEXTURE (memory_texture));

  switch (format)
    {
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      data = gdk_memory_texture_get_data (GDK_MEMORY_TEXTURE (memory_texture));
      stride = gdk_memory_texture_get_stride (GDK_MEMORY_TEXTURE (memory_texture));
      image.format = PNG_FORMAT_RGBA;
      break;

    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
      stride = image.width * 4;
      new_data = g_malloc (stride * image.height);
      gdk_texture_download (memory_texture, new_data, stride);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      flip_02 (new_data, image.width, image.height, stride);
#endif
      data = new_data;
      image.format = PNG_FORMAT_RGBA;
      break;

    case GDK_MEMORY_R16G16B16:
      data = gdk_memory_texture_get_data (GDK_MEMORY_TEXTURE (memory_texture));
      stride = gdk_memory_texture_get_stride (GDK_MEMORY_TEXTURE (memory_texture));
      image.format = PNG_FORMAT_LINEAR_RGB;
      break;

    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      data = gdk_memory_texture_get_data (GDK_MEMORY_TEXTURE (memory_texture));
      stride = gdk_memory_texture_get_stride (GDK_MEMORY_TEXTURE (memory_texture));
      image.format = PNG_FORMAT_LINEAR_RGB_ALPHA;
      break;

    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      /* This isn't very efficient */
      new_data = g_malloc (image.width * image.height * 16);
      gdk_texture_download_float (memory_texture, (float *)new_data, image.width * 16);
      convert_float_to_16bit_inplace ((float *)new_data, image.width, image.height);
      data = new_data;
      stride = image.width * 8;
      image.format = PNG_FORMAT_LINEAR_RGB_ALPHA;
      break;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
    }

  if (image.format & PNG_FORMAT_FLAG_LINEAR)
    stride /= 2;

  png_image_write_get_memory_size (image, size, FALSE, data, stride, NULL);

  buffer = g_malloc (size);
  result = png_image_write_to_memory (&image, buffer, &size, FALSE, data, stride, NULL);

  g_assert (result);

  g_object_unref (memory_texture);
  png_image_free (&image);
  g_free (new_data);

  return g_bytes_new_take (buffer, size);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */

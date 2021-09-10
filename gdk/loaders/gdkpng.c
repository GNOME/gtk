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

/* {{{ Callbacks */

/* No sigsetjmp on Windows */
#ifndef HAVE_SIGSETJMP
#define sigjmp_buf jmp_buf
#define sigsetjmp(jb, x) setjmp(jb)
#define siglongjmp longjmp
#endif

typedef struct
{
  guchar *data;
  gsize size;
  gsize position;
} png_io;


static void
png_read_func (png_structp png,
               png_bytep   data,
               png_size_t  size)
{
  png_io *io;

  io = png_get_io_ptr (png);

  if (io->position + size > io->size)
    png_error (png, "Read past EOF");

  memcpy (data, io->data + io->position, size);
  io->position += size;
}

static void
png_write_func (png_structp png,
                png_bytep   data,
                png_size_t  size)
{
  png_io *io;

  io = png_get_io_ptr (png);

  if (io->position > io->size ||
      io->size - io->position  < size)
    {
      io->size = io->position + size;
      io->data = g_realloc (io->data, io->size);
    }

  memcpy (io->data + io->position, data, size);
  io->position += size;
}

static void
png_flush_func (png_structp png)
{
}

static png_voidp
png_malloc_callback (png_structp o,
                     png_size_t  size)
{
  return g_try_malloc (size);
}

static void
png_free_callback (png_structp o,
                   png_voidp   x)
{
  g_free (x);
}

static void
png_simple_error_callback (png_structp     png,
                           png_const_charp error_msg)
{
  GError **error = png_get_error_ptr (png);

  if (error && !*error)
    g_set_error (error,
                 GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                 "Error reading png (%s)", error_msg);

  longjmp (png_jmpbuf (png), 1);
}

static void
png_simple_warning_callback (png_structp     png,
                             png_const_charp error_msg)
{
}

/* }}} */
/* {{{ Format conversion */

static void
unpremultiply (guchar *data,
               int     width,
               int     height,
               int     stride)
{
  gsize x, y;

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          guchar *b = &data[x * 4];
          guint32 pixel;
          guchar alpha;

          memcpy (&pixel, b, sizeof (guint32));
          alpha = (pixel & 0xff000000) >> 24;
          if (alpha == 0)
            {
              b[0] = 0;
              b[1] = 0;
              b[2] = 0;
              b[3] = 0;
            }
          else
            {
              b[0] = (((pixel & 0x00ff0000) >> 16) * 255 + alpha / 2) / alpha;
              b[1] = (((pixel & 0x0000ff00) >>  8) * 255 + alpha / 2) / alpha;
              b[2] = (((pixel & 0x000000ff) >>  0) * 255 + alpha / 2) / alpha;
              b[3] = alpha;
            }
        }
      data += stride;
    }
}

static inline int
multiply_alpha (int alpha, int color)
{
  int temp = (alpha * color) + 0x80;
  return ((temp + (temp >> 8)) >> 8);
}

static void
premultiply_data (png_structp   png,
                  png_row_infop row_info,
                  png_bytep     data)
{
  unsigned int i;

  for (i = 0; i < row_info->rowbytes; i += 4)
    {
      uint8_t *base  = &data[i];
      uint8_t  alpha = base[3];
      uint32_t p;

      if (alpha == 0)
        {
          p = 0;
        }
      else
        {
          uint8_t  red   = base[0];
          uint8_t  green = base[1];
          uint8_t  blue  = base[2];

          if (alpha != 0xff)
            {
              red   = multiply_alpha (alpha, red);
              green = multiply_alpha (alpha, green);
              blue  = multiply_alpha (alpha, blue);
            }
          p = ((uint32_t)alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
        }
      memcpy (base, &p, sizeof (uint32_t));
  }
}

static void
convert_bytes_to_data (png_structp   png,
                       png_row_infop row_info,
                       png_bytep     data)
{
  unsigned int i;

  for (i = 0; i < row_info->rowbytes; i += 4)
    {
      uint8_t *base  = &data[i];
      uint8_t  red   = base[0];
      uint8_t  green = base[1];
      uint8_t  blue  = base[2];
      uint32_t pixel;

      pixel = (0xffu << 24) | (red << 16) | (green << 8) | (blue << 0);
      memcpy (base, &pixel, sizeof (uint32_t));
    }
}

/* }}} */
/* {{{ Public API */

GdkTexture *
gdk_load_png (GBytes  *bytes,
              GError **error)
{
  png_io io;
  png_struct *png = NULL;
  png_info *info;
  guint width, height;
  int depth, color_type;
  int interlace, stride;
  GdkMemoryFormat format;
  guchar *buffer = NULL;
  guchar **row_pointers = NULL;
  GBytes *out_bytes;
  GdkTexture *texture;

  io.data = (guchar *)g_bytes_get_data (bytes, &io.size);
  io.position = 0;

  png = png_create_read_struct_2 (PNG_LIBPNG_VER_STRING,
                                  error,
                                  png_simple_error_callback,
                                  png_simple_warning_callback,
                                  NULL,
                                  png_malloc_callback,
                                  png_free_callback);
  if (png == NULL)
    {
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_INSUFFICIENT_MEMORY,
                   "Failed to parse png image");
      return NULL;
    }

  info = png_create_info_struct (png);
  if (info == NULL)
    {
      png_destroy_read_struct (&png, NULL, NULL);
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_INSUFFICIENT_MEMORY,
                   "Failed to parse png image");
      return NULL;
    }

  png_set_read_fn (png, &io, png_read_func);

  if (sigsetjmp (png_jmpbuf (png), 1))
    {
      g_free (buffer);
      g_free (row_pointers);
      png_destroy_read_struct (&png, &info, NULL);
      return NULL;
    }

  png_read_info (png, info);

  png_get_IHDR (png, info,
                &width, &height, &depth,
                &color_type, &interlace, NULL, NULL);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb (png);

  if (color_type == PNG_COLOR_TYPE_GRAY)
    png_set_expand_gray_1_2_4_to_8 (png);

  if (png_get_valid (png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha (png);

  if (depth < 8)
    png_set_packing (png);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb (png);

  if (interlace != PNG_INTERLACE_NONE)
    png_set_interlace_handling (png);

  png_set_filler (png, 0xff, PNG_FILLER_AFTER);

  png_read_update_info (png, info);
  png_get_IHDR (png, info,
                &width, &height, &depth,
                &color_type, &interlace, NULL, NULL);
  if ((depth != 8) ||
      !(color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_RGB_ALPHA))
    {
      png_destroy_read_struct (&png, &info, NULL);
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED,
                   "Failed to parse png image");
      return NULL;
    }

  switch (color_type)
    {
    case PNG_COLOR_TYPE_RGB_ALPHA:
      format = GDK_MEMORY_DEFAULT;
      png_set_read_user_transform_fn (png, premultiply_data);
      break;
    case PNG_COLOR_TYPE_RGB:
      format = GDK_MEMORY_DEFAULT;
      png_set_read_user_transform_fn (png, convert_bytes_to_data);
      break;
    default:
      g_assert_not_reached ();
    }

  stride = width * gdk_memory_format_bytes_per_pixel (format);
  if (stride % 4)
    stride += 4 - stride % 4;

  buffer = g_try_malloc_n (height, stride);
  row_pointers = g_try_malloc_n (height, sizeof (char *));

  if (!buffer || !row_pointers)
    {
      g_free (buffer);
      g_free (row_pointers);
      png_destroy_read_struct (&png, &info, NULL);
      g_set_error_literal (error,
                           GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_INSUFFICIENT_MEMORY,
                           "Not enough memory to load png");
      return NULL;
    }

  for (int i = 0; i < height; i++)
    row_pointers[i] = &buffer[i * stride];

  png_read_image (png, row_pointers);
  png_read_end (png, info);

  out_bytes = g_bytes_new_take (buffer, height * stride);
  texture = gdk_memory_texture_new (width, height,
                                    format,
                                    out_bytes, stride);
  g_bytes_unref (out_bytes);

  g_free (row_pointers);
  png_destroy_read_struct (&png, &info, NULL);

  return texture;
}

GBytes *
gdk_save_png (GdkTexture *texture)
{
  png_struct *png = NULL;
  png_info *info;
  png_io io;
  guint width, height;
  int stride;
  guchar *data = NULL;
  guchar *row;
  int y;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  stride = width * 4;
  data = g_malloc_n (stride, height);
  gdk_texture_download (texture, data, stride);
  unpremultiply (data, width, height, stride);

  png = png_create_write_struct_2 (PNG_LIBPNG_VER_STRING, NULL,
                                   png_simple_error_callback,
                                   png_simple_warning_callback,
                                   NULL,
                                   png_malloc_callback,
                                   png_free_callback);
  if (!png)
    return NULL;

  info = png_create_info_struct (png);
  if (!info)
    {
      png_destroy_read_struct (&png, NULL, NULL);
      return NULL;
    }

  if (sigsetjmp (png_jmpbuf (png), 1))
    {
      png_destroy_read_struct (&png, &info, NULL);
      return NULL;
    }

  io.data = NULL;
  io.size = 0;
  io.position = 0;

  png_set_write_fn (png, &io, png_write_func, png_flush_func);

  png_set_IHDR (png, info, width, height, 8,
                PNG_COLOR_TYPE_RGB_ALPHA,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_BASE,
                PNG_FILTER_TYPE_BASE);

  png_write_info (png, info);
  png_set_packing (png);

  for (y = 0, row = data; y < height; y++, row += stride)
    png_write_rows (png, &row, 1);

  png_write_end (png, info);

  png_destroy_write_struct (&png, &info);

  return g_bytes_new_take (io.data, io.size);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */

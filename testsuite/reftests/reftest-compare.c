/*
 * Copyright (C) 2011,2021 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "reftest-compare.h"

static gboolean
memory_format_is_high_depth (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_X8B8G8R8:
    case GDK_MEMORY_G8:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8:
      return FALSE;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_A16:
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_A16_FLOAT:
    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_A32_FLOAT:
      return TRUE;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

/* Compares two GDK_MEMORY_DEFAULT buffers, returning NULL if the
 * buffers are equal or a surface containing a diff between the two
 * surfaces.
 *
 * This function is originally from cairo:test/buffer-diff.c.
 * Copyright © 2004 Richard D. Worth
 */
static GdkTexture *
buffer_diff_u8 (GdkColorState *color_state,
                const guchar  *buf_a,
                int            stride_a,
                const guchar  *buf_b,
                int            stride_b,
                int            width,
                int            height)
{
  int x, y;
  guchar *buf_diff = NULL;
  int stride_diff = 0;
  GdkTexture *diff = NULL;

  for (y = 0; y < height; y++)
    {
      const guint32 *row_a = (const guint32 *) (buf_a + y * stride_a);
      const guint32 *row_b = (const guint32 *) (buf_b + y * stride_b);
      guint32 *row = (guint32 *) (buf_diff + y * stride_diff);

      for (x = 0; x < width; x++)
        {
          int channel;
          guint32 diff_pixel = 0;

          /* check if the pixels are the same */
          if (row_a[x] == row_b[x])
            continue;

          /* even if they're not literally the same, fully-transparent
           * pixels are effectively the same regardless of colour */
          if ((row_a[x] & 0xff000000) == 0 && (row_b[x] & 0xff000000) == 0)
            continue;

          if (diff == NULL)
            {
              GdkMemoryTextureBuilder *builder;
              GBytes *bytes;

              stride_diff = 4 * width;
              buf_diff = g_malloc0_n (stride_diff, height);
              bytes = g_bytes_new_take (buf_diff, stride_diff * height);
              builder = gdk_memory_texture_builder_new ();
              gdk_memory_texture_builder_set_width (builder, width);
              gdk_memory_texture_builder_set_height (builder, height);
              gdk_memory_texture_builder_set_format (builder, GDK_MEMORY_DEFAULT);
              gdk_memory_texture_builder_set_color_state (builder, color_state);
              gdk_memory_texture_builder_set_bytes (builder, bytes);
              gdk_memory_texture_builder_set_stride (builder, stride_diff);
              diff = gdk_memory_texture_builder_build (builder);
              g_object_unref (builder);
              row = (guint32 *) (buf_diff + y * stride_diff);
            }

          /* calculate a difference value for all 4 channels */
          for (channel = 0; channel < 4; channel++)
            {
              int value_a = (row_a[x] >> (channel*8)) & 0xff;
              int value_b = (row_b[x] >> (channel*8)) & 0xff;
              guint channel_diff;

              channel_diff = ABS (value_a - value_b);
              channel_diff *= 4;  /* emphasize */
              if (channel_diff)
                channel_diff += 128; /* make sure it's visible */
              if (channel_diff > 255)
                channel_diff = 255;
              diff_pixel |= channel_diff << (channel * 8);
            }

          if ((diff_pixel & 0x00ffffff) == 0)
            {
              /* alpha only difference, convert to luminance */
              guint8 alpha = diff_pixel >> 24;
              diff_pixel = alpha * 0x010101;
            }
          /* make the pixel fully opaque */
          diff_pixel |= 0xff000000;
          
          row[x] = diff_pixel;
      }
  }

  return diff;
}

/* Compares two FLOAT buffers, returning NULL if the
 * buffers are equal or a surface containing a diff between the two
 * surfaces.
 */
static GdkTexture *
buffer_diff_float (GdkColorState *color_state,
                   const guchar  *buf_a,
                   int            stride_a,
                   const guchar  *buf_b,
                   int            stride_b,
                   int            width,
                   int            height)
{
  int x, y;
  guchar *buf_diff = NULL;
  int stride_diff = 0;
  GdkTexture *diff = NULL;

  for (y = 0; y < height; y++)
    {
      const float *row_a = (const float *) (buf_a + y * stride_a);
      const float *row_b = (const float *) (buf_b + y * stride_b);
      float *row = (float *) (buf_diff + y * stride_diff);

      for (x = 0; x < width; x++)
        {
          int channel;

          /* check if the pixels are the same */
          if (G_APPROX_VALUE (row_a[4 * x + 0], row_b[4 * x + 0], 1. / 255) &&
              G_APPROX_VALUE (row_a[4 * x + 1], row_b[4 * x + 1], 1. / 255) &&
              G_APPROX_VALUE (row_a[4 * x + 2], row_b[4 * x + 2], 1. / 255) &&
              G_APPROX_VALUE (row_a[4 * x + 3], row_b[4 * x + 3], 1. / 255))
            continue;

          /* even if they're not literally the same, fully-transparent
           * pixels are effectively the same regardless of colour */
          if (G_APPROX_VALUE (row_a[4 * x + 3], 0.0, 1. / 255) &&
              G_APPROX_VALUE (row_b[4 * x + 3], 0.0, 1. / 255))
            continue;

          if (diff == NULL)
            {
              GdkMemoryTextureBuilder *builder;
              GBytes *bytes;

              stride_diff = 4 * width * sizeof (float);
              buf_diff = g_malloc0_n (stride_diff, height);
              bytes = g_bytes_new_take (buf_diff, stride_diff * height);
              builder = gdk_memory_texture_builder_new ();
              gdk_memory_texture_builder_set_width (builder, width);
              gdk_memory_texture_builder_set_height (builder, height);
              gdk_memory_texture_builder_set_format (builder, GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED);
              gdk_memory_texture_builder_set_color_state (builder, color_state);
              gdk_memory_texture_builder_set_bytes (builder, bytes);
              gdk_memory_texture_builder_set_stride (builder, stride_diff);
              diff = gdk_memory_texture_builder_build (builder);
              g_object_unref (builder);
              row = (float *) (buf_diff + y * stride_diff);
            }

          /* calculate a difference value for all 4 channels */
          for (channel = 0; channel < 4; channel++)
            {
              float value_a = row_a[4 * x + channel];
              float value_b = row_b[4 * x + channel];\
              float channel_diff;

              channel_diff = ABS (value_a - value_b);
              channel_diff *= 4;  /* emphasize */
              if (channel_diff)
                channel_diff += 0.5; /* make sure it's visible */
              if (channel_diff > 1.0)
                channel_diff = 1.0;
              row[4 * x + channel] = channel_diff;
            }

          if (row[4 * x + 0] < 0.5 &&
              row[4 * x + 1] < 0.5 &&
              row[4 * x + 2] < 0.5)
            {
              /* alpha only difference, convert to luminance */
              row[4 * x + 0] = row[4 * x + 3];
              row[4 * x + 1] = row[4 * x + 3];
              row[4 * x + 2] = row[4 * x + 3];
            }
          /* make the pixel fully opaque */
          row[4 * x + 3] = 1.0;
      }
  }

  return diff;
}

GdkTexture *
reftest_compare_textures (GdkTexture *texture1,
                          GdkTexture *texture2)
{
  int w, h, stride;
  guchar *data1, *data2;
  GdkTextureDownloader *downloader;
  GdkColorState *color_state;
  GdkTexture *diff;
  gboolean high_depth;
  
  w = MAX (gdk_texture_get_width (texture1), gdk_texture_get_width (texture2));
  h = MAX (gdk_texture_get_height (texture1), gdk_texture_get_height (texture2));
  color_state = gdk_texture_get_color_state (texture1);

  downloader = gdk_texture_downloader_new (texture1);
  gdk_texture_downloader_set_color_state (downloader, color_state);
  high_depth = memory_format_is_high_depth (gdk_texture_get_format (texture1)) ||
               memory_format_is_high_depth (gdk_texture_get_format (texture1));
  if (high_depth)
    {
      gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED);
      stride = w *  4 * sizeof (float);
    }
  else
    {
      gdk_texture_downloader_set_format (downloader, GDK_MEMORY_DEFAULT);
      stride = w *  4 * sizeof (float);
    }

  data1 = g_malloc_n (stride, h);
  gdk_texture_downloader_download_into (downloader, data1, stride);
  data2 = g_malloc_n (stride, h);
  gdk_texture_downloader_set_texture (downloader, texture2);
  gdk_texture_downloader_download_into (downloader, data2, stride);

  if (high_depth)
    {
      diff = buffer_diff_float (color_state,
                                data1, stride,
                                data2, stride,
                                w, h);
    }
  else
    {
      diff = buffer_diff_u8 (color_state,
                             data1, stride,
                             data2, stride,
                             w, h);
    }

  g_free (data1);
  g_free (data2);

  return diff;
}

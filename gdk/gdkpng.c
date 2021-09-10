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

#include "gdkpng.h"

#include "gdktexture.h"
#include "gdkmemorytextureprivate.h"
#include "gsk/ngl/fp16private.h"
#include <png.h>
#include <stdio.h>

/* {{{ IO handling */

#ifdef HAVE_FOPENCOOKIE

static ssize_t
read_stream (void   *cookie,
             char   *buf,
             size_t  size)
{
  GInputStream *stream = cookie;

  return g_input_stream_read (stream, buf, size, NULL, NULL);
}

static ssize_t
write_stream (void       *cookie,
              const char *buf,
              size_t      size)
{
  GOutputStream *stream = cookie;

  return g_output_stream_write (stream, buf, size, NULL, NULL);
}

static int
seek_stream (void    *cookie,
             off64_t *offset,
             int      whence)
{
  GSeekable *seekable = cookie;
  GSeekType seek_type;

  if (whence == SEEK_SET)
    seek_type = G_SEEK_SET;
  else if (whence == SEEK_CUR)
    seek_type = G_SEEK_CUR;
  else if (whence == SEEK_END)
    seek_type = G_SEEK_END;
  else
    g_assert_not_reached ();

  if (g_seekable_seek (seekable, *offset, seek_type, NULL, NULL))
    {
      *offset = g_seekable_tell (seekable);
      return 0;
    }

  return -1;
}

static int
close_stream (void *cookie)
{
  GObject *stream = cookie;

  g_object_unref (stream);

  return 0;
}

static cookie_io_functions_t cookie_funcs = {
  read_stream,
  write_stream,
  seek_stream,
  close_stream
};

#else

static GBytes *
read_all_data (GInputStream  *source,
               GError       **error)
{
  GOutputStream *output;
  gssize size;
  GBytes *bytes;

  output = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  size = g_output_stream_splice (output, source, 0, NULL, error);
  if (size == -1)
    {
      g_object_unref (output);
      return NULL;
    }

  g_output_stream_close (output, NULL, NULL);
  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (output));
  g_object_unref (output);

  return bytes;
}

#endif

/* }}} */
/* {{{ Format conversion */

static void
convert_half_float (guchar            *dest_data,
                    gsize              dest_stride,
                    GdkMemoryFormat    dest_format,
                    const guchar      *src_data,
                    gsize              src_stride,
                    GdkMemoryFormat    src_format,
                    gsize              width,
                    gsize              height)
{
  gsize x, y;
  guint16 *dest;
  const guint16 *src;
  float *c;

  c = g_malloc (width * 4 * sizeof (float));
  for (y = 0; y < height; y++)
    {
      dest = (guint16 *)dest_data;
      src = (const guint16 *)src_data;

      half_to_float (src, c, width);
      for (x = 0; x < width; x++)
        {
          dest[4 * x    ] = (guint16)(65535 * c[4 * x    ]);
          dest[4 * x + 1] = (guint16)(65535 * c[4 * x + 1]);
          dest[4 * x + 2] = (guint16)(65535 * c[4 * x + 2]);
          if (src_format == GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED)
            dest[4 * x + 3] = (guint16)(65535 * c[4 * x + 3]);
          else
            dest[4 * x + 3] = 65535;
        }
      dest_data += dest_stride;
      src_data += src_stride;
    }
  g_free (c);
}

static void
convert_float (guchar            *dest_data,
               gsize              dest_stride,
               GdkMemoryFormat    dest_format,
               const guchar      *src_data,
               gsize              src_stride,
               GdkMemoryFormat    src_format,
               gsize              width,
               gsize              height)
{
  gsize x, y;
  guint16 *dest;
  const float *src;

  for (y = 0; y < height; y++)
    {
      dest = (guint16 *)dest_data;
      src = (const float *)src_data;
      for (x = 0; x < width; x++)
        {
          dest[4 * x    ] = (guint16)(65535 * src[4 * x    ]);
          dest[4 * x + 1] = (guint16)(65535 * src[4 * x + 1]);
          dest[4 * x + 2] = (guint16)(65535 * src[4 * x + 2]);
          if (src_format == GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED)
            dest[4 * x + 3] = (guint16)(65535 * src[4 * x + 3]);
          else
            dest[4 * x + 3] = 65535;
        }
      dest_data += dest_stride;
      src_data += src_stride;
    }
}

static void
convert (guchar            *dest_data,
         gsize              dest_stride,
         GdkMemoryFormat    dest_format,
         const guchar      *src_data,
         gsize              src_stride,
         GdkMemoryFormat    src_format,
         gsize              width,
         gsize              height)
{
  if (dest_format < 3)
    gdk_memory_convert (dest_data, dest_stride, dest_format,
                        src_data, src_stride, src_format,
                        width, height);
  else
    {
      g_assert (dest_format == GDK_MEMORY_R16G16B16A16_PREMULTIPLIED);

      switch ((int)src_format)
        {
        case GDK_MEMORY_R16G16B16_FLOAT:
        case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
          convert_half_float (dest_data, dest_stride, dest_format,
                              src_data, src_stride, src_format,
                              width, height);
          break;
        case GDK_MEMORY_R32G32B32_FLOAT:
        case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
          convert_float (dest_data, dest_stride, dest_format,
                         src_data, src_stride, src_format,
                         width, height);
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

/* }}} */
/* {{{ Public API */

GdkTexture *
gdk_load_png (GInputStream  *stream,
              GError       **error)
{
  png_image image = { NULL, PNG_IMAGE_VERSION, 0, };
  gsize size;
  gsize stride;
  guint16 *buffer;
  GBytes *bytes;
  GdkTexture *texture;

#ifdef HAVE_FOPENCOOKIE
  FILE *file = fopencookie (g_object_ref (stream), "r", cookie_funcs);

  png_image_begin_read_from_stdio (&image, file);
#else
  GBytes *data;

  data = read_all_data (stream, error);
  if (!data)
    return NULL;
  png_image_begin_read_from_memory (&image,
                                    g_bytes_get_data (data, NULL),
                                    g_bytes_get_size (data));
#endif

  image.format = PNG_FORMAT_LINEAR_RGB_ALPHA;

  stride = PNG_IMAGE_ROW_STRIDE (image);
  size = PNG_IMAGE_BUFFER_SIZE (image, stride);
  buffer = g_malloc (size);

  png_image_finish_read (&image, NULL, buffer, stride, NULL);

#ifdef HAVE_FOPENCOOKIE
  fclose (file);
#else
  g_bytes_unref (data);
#endif

  bytes = g_bytes_new_take (buffer, size);

  texture = gdk_memory_texture_new (image.width, image.height,
                                    GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
                                    bytes, 2 * stride);

  g_bytes_unref (bytes);

  png_image_free (&image);

  return texture;
}

gboolean
gdk_save_png (GOutputStream    *stream,
              const guchar     *data,
              int               width,
              int               height,
              int               stride,
              GdkMemoryFormat   format,
              GError          **error)
{
  png_image image = { NULL, PNG_IMAGE_VERSION, 0, };
  gboolean result;
  guchar *new_data = NULL;

  switch ((int)format)
    {
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
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
      stride = width * 4;
      new_data = g_malloc (stride * height);
      convert (new_data, stride, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
               data, width * gdk_memory_format_bytes_per_pixel (format), format,
               width, height);
      data = new_data;
      image.format = PNG_FORMAT_RGBA;
      break;
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      image.format = PNG_FORMAT_LINEAR_RGB_ALPHA;
      break;
    case GDK_MEMORY_R16G16B16:
      image.format = PNG_FORMAT_LINEAR_RGB;
      break;
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      stride = width * 8;
      new_data = g_malloc (stride * height);
      convert (new_data, stride, GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
               data, width * gdk_memory_format_bytes_per_pixel (format), format,
               width, height);
      image.format = PNG_FORMAT_LINEAR_RGB_ALPHA;
      break;
      break;
    default:
      g_assert_not_reached ();
    }

  if (image.format & PNG_FORMAT_FLAG_LINEAR)
    stride /= 2;

#ifdef HAVE_FOPENCOOKIE
  FILE *file = fopencookie (g_object_ref (stream), "w", cookie_funcs);
  result = png_image_write_to_stdio (&image, file, FALSE, data, stride, NULL);
  fclose (file);
#else
  gsize written;
  png_alloc_size_t size;
  gpointer buffer;
  png_image_write_get_memory_size (image, size, FALSE, data, stride, NULL);
  buffer = g_malloc (size);
  result = png_image_write_to_memory (&image, buffer, &size, FALSE, data, stride, NULL);
  if (result)
    result = g_output_stream_write_all (stream, buffer, (gsize)size, &written, NULL, NULL);
  g_free (buffer);
#endif

  if (!result)
    {
      g_set_error_literal (error,
                           G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Saving png failed");
    }

  png_image_free (&image);

  g_free (new_data);

  return result;
}

/* }}} */
/* {{{ Async code */

static void
load_png_in_thread (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GInputStream *stream = source_object;
  GdkTexture *texture;
  GError *error = NULL;

  texture = gdk_load_png (stream, &error);

  if (texture)
    g_task_return_pointer (task, texture, g_object_unref);
  else
    g_task_return_error (task, error);
}

void
gdk_load_png_async (GInputStream         *stream,
                    GCancellable         *cancellable,
                    GAsyncReadyCallback   callback,
                    gpointer              user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_run_in_thread (task, load_png_in_thread);
  g_object_unref (task);
}

GdkTexture *
gdk_load_png_finish (GAsyncResult  *result,
                     GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

typedef struct {
  const guchar *data;
  int width;
  int height;
  int stride;
  GdkMemoryFormat format;
} SavePngData;

static void
save_png_in_thread (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GOutputStream *stream = source_object;
  SavePngData *data = task_data;
  GError *error = NULL;
  gboolean result;

  result = gdk_save_png (stream,
                         data->data,
                         data->width,
                         data->height,
                         data->stride,
                         data->format,
                         &error);

  if (result)
    g_task_return_boolean (task, result);
  else
    g_task_return_error (task, error);
}

void
gdk_save_png_async (GOutputStream          *stream,
                    const guchar           *data,
                    int                     width,
                    int                     height,
                    int                     stride,
                    GdkMemoryFormat         format,
                    GCancellable           *cancellable,
                    GAsyncReadyCallback     callback,
                    gpointer                user_data)
{
  GTask *task;
  SavePngData *save_data;

  save_data = g_new0 (SavePngData, 1);
  save_data->data = data;
  save_data->width = width;
  save_data->height = height;
  save_data->stride = stride;
  save_data->format = format;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_task_data (task, save_data, g_free);
  g_task_run_in_thread (task, save_png_in_thread);
  g_object_unref (task);
}

gboolean
gdk_save_png_finish (GAsyncResult  *result,
                     GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */

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

#include "gdktiff.h"

#include "gdktexture.h"
#include "gdkmemorytextureprivate.h"
#include <tiffio.h>

/* {{{ IO handling */

typedef struct
{
  GObject *stream;
  GInputStream *input;
  GOutputStream *output;

  gchar *buffer;
  gsize allocated;
  gsize used;
  gsize position;
} TiffIO;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"

static void
tiff_io_warning (const char *module,
                 const char *fmt,
                 va_list     ap)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, fmt, ap);
}

static void
tiff_io_error (const char *module,
               const char *fmt,
               va_list     ap)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, fmt, ap);
}

#pragma GCC diagnostic pop

static tsize_t
tiff_io_read (thandle_t handle,
              tdata_t   buffer,
              tsize_t   size)
{
  TiffIO *io = (TiffIO *) handle;
  GError *error = NULL;
  gssize read  = -1;
  gsize bytes_read = 0;

  if (! g_input_stream_read_all (io->input,
                                 (void *) buffer, (gsize) size,
                                 &bytes_read,
                                 NULL, &error))
    {
      g_printerr ("%s", error->message);
      g_clear_error (&error);
    }

  read = bytes_read;

  return (tsize_t) read;
}

static tsize_t
tiff_io_write (thandle_t handle,
               tdata_t   buffer,
               tsize_t   size)
{
  TiffIO *io = (TiffIO *) handle;
  GError *error = NULL;
  gssize written = -1;
  gsize bytes_written = 0;

  if (! g_output_stream_write_all (io->output,
                                   (void *) buffer, (gsize) size,
                                   &bytes_written,
                                   NULL, &error))
    {
      g_printerr ("%s", error->message);
      g_clear_error (&error);
    }

  written = bytes_written;

  return (tsize_t) written;
}

static GSeekType
lseek_to_seek_type (int whence)
{
  switch (whence)
    {
    default:
    case SEEK_SET:
      return G_SEEK_SET;
    case SEEK_CUR:
      return G_SEEK_CUR;
    case SEEK_END:
      return G_SEEK_END;
    }
}

static toff_t
tiff_io_seek (thandle_t handle,
              toff_t    offset,
              int       whence)
{
  TiffIO *io = (TiffIO *) handle;
  GError *error = NULL;
  gboolean sought = FALSE;
  goffset position = -1;

  sought = g_seekable_seek (G_SEEKABLE (io->stream),
                            (goffset) offset, lseek_to_seek_type (whence),
                            NULL, &error);
  if (sought)
    {
      position = g_seekable_tell (G_SEEKABLE (io->stream));
    }
  else
    {
      g_printerr ("%s", error->message);
      g_clear_error (&error);
    }

  return (toff_t) position;
}

static int
tiff_io_close (thandle_t handle)
{
  TiffIO *io = (TiffIO *) handle;
  GError *error = NULL;
  gboolean closed = FALSE;

  if (io->input)
    closed = g_input_stream_close (io->input, NULL, &error);
  else if (io->output)
    closed = g_output_stream_close (io->output, NULL, &error);

  if (!closed)
    {
      g_printerr ("%s", error->message);
      g_clear_error (&error);
    }

  g_clear_object (&io->stream);
  io->input = NULL;
  io->output = NULL;

  g_clear_pointer (&io->buffer, g_free);

  io->allocated = 0;
  io->used = 0;
  io->position  = 0;

  g_free (io);

  return closed ? 0 : -1;
}

static goffset
input_stream_query_size (GInputStream *stream)
{
  goffset size = 0;

  while (G_IS_FILTER_INPUT_STREAM (stream))
    stream = g_filter_input_stream_get_base_stream (G_FILTER_INPUT_STREAM (stream));

  if (G_IS_FILE_INPUT_STREAM (stream))
    {
      GFileInfo *info;

      info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM (stream),
                                             G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                             NULL,
                                             NULL);
      if (info)
        {
          size = g_file_info_get_size (info);
          g_object_unref (info);
        }
    }

  return size;
}

static goffset
output_stream_query_size (GOutputStream *stream)
{
  goffset size = 0;

  while (G_IS_FILTER_OUTPUT_STREAM (stream))
    stream = g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (stream));

  if (G_IS_FILE_OUTPUT_STREAM (stream))
    {
      GFileInfo *info;

      info = g_file_output_stream_query_info (G_FILE_OUTPUT_STREAM (stream),
                                              G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                              NULL,
                                              NULL);
      if (info)
        {
          size = g_file_info_get_size (info);
          g_object_unref (info);
        }
    }

  return size;
}

static toff_t
tiff_io_get_file_size (thandle_t handle)
{
  TiffIO *io = (TiffIO *) handle;

  if (io->input)
    return (toff_t) input_stream_query_size (io->input);
  else if (io->output)
    return (toff_t) output_stream_query_size (io->output);

  return (toff_t) 0;
}

static TIFF *
tiff_open (gpointer      stream,
           const gchar  *mode,
           GError      **error)
{
  TIFFSetWarningHandler ((TIFFErrorHandler) tiff_io_warning);
  TIFFSetErrorHandler ((TIFFErrorHandler) tiff_io_error);
  TiffIO *io;

  io = g_new0 (TiffIO, 1);

  if (strcmp (mode, "r") == 0)
    {
      io->input = G_INPUT_STREAM (stream);
      io->stream = g_object_ref (G_OBJECT (stream));
    }
  else if (strcmp (mode, "w") == 0)
    {
      io->output = G_OUTPUT_STREAM (stream);
      io->stream = g_object_ref (G_OBJECT (stream));
    }
  else
    {
      g_assert_not_reached ();
    }

  return TIFFClientOpen ("GTK", mode,
                         (thandle_t) io,
                         tiff_io_read,
                         tiff_io_write,
                         tiff_io_seek,
                         tiff_io_close,
                         tiff_io_get_file_size,
                         NULL, NULL);
}

/* }}} */
/* {{{ Public API */

static struct {
  GdkMemoryFormat format;
  guint16 bits_per_sample;
  guint16 samples_per_pixel;
  guint16 sample_format;
} format_data[] = {
  { GDK_MEMORY_DEFAULT,                     8, 4, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R8G8B8,                      8, 3, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, 16, 4, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R16G16B16_FLOAT,            16, 3, SAMPLEFORMAT_IEEEFP },
  { GDK_MEMORY_R32G32B32_FLOAT,            32, 3, SAMPLEFORMAT_IEEEFP },
};

gboolean
gdk_save_tiff (GOutputStream    *stream,
               const guchar     *data,
               int               width,
               int               height,
               int               stride,
               GdkMemoryFormat   format,
               GError          **error)
{
  TIFF *tif;
  guint16 bits_per_sample = 0;
  guint16 samples_per_pixel = 0;
  guint16 sample_format = 0;
  const guchar *line;

  tif = tiff_open (stream, "w", error);
  if (!tif)
    return FALSE;

  for (int i = 0; i < G_N_ELEMENTS (format_data); i++)
    {
      if (format == format_data[i].format)
        {
          bits_per_sample = format_data[i].bits_per_sample;
          samples_per_pixel = format_data[i].samples_per_pixel;
          sample_format = format_data[i].sample_format;
          break;
        }
    }

  g_assert (bits_per_sample != 0);

  TIFFSetField (tif, TIFFTAG_SOFTWARE, "GTK");
  TIFFSetField (tif, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField (tif, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField (tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
  TIFFSetField (tif, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
  TIFFSetField (tif, TIFFTAG_SAMPLEFORMAT, sample_format);
  TIFFSetField (tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField (tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
  // TODO: save gamma / colorspace

  if (samples_per_pixel > 3)
    {
      guint16 extra_samples[] = { EXTRASAMPLE_ASSOCALPHA };
      TIFFSetField (tif, TIFFTAG_EXTRASAMPLES, 1, extra_samples);
    }

  TIFFSetField (tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  TIFFSetField (tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  line = (const guchar *)data;
  for (int y = 0; y < height; y++)
    {
      if (TIFFWriteScanline (tif, (void *)line, y, 0) == -1)
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Writing data failed at row %d", y);
          TIFFClose (tif);
          return FALSE;
        }

      line += stride;
    }

  TIFFFlushData (tif);
  TIFFClose (tif);

  return TRUE;
}

/* This isn't meant to be a very versatile tiff loader.
 * It just aims to load the subset that we're saving
 * ourselves, above.
 */
GdkTexture *
gdk_load_tiff (GInputStream     *stream,
               GError          **error)
{
  TIFF *tif;
  guint16 samples_per_pixel;
  guint16 bits_per_sample;
  guint16 photometric;
  guint16 planarconfig;
  guint16 sample_format;
  guint16 orientation;
  guint32 width, height;
  GdkMemoryFormat format;
  guchar *data, *line;
  gsize stride;
  int bpp;
  GBytes *bytes;
  GdkTexture *texture;

  tif = tiff_open (stream, "r", error);
  if (!tif)
    return NULL;

  TIFFSetDirectory (tif, 0);

  TIFFGetFieldDefaulted (tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
  TIFFGetFieldDefaulted (tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
  TIFFGetFieldDefaulted (tif, TIFFTAG_SAMPLEFORMAT, &sample_format);
  TIFFGetFieldDefaulted (tif, TIFFTAG_PHOTOMETRIC, &photometric);
  TIFFGetFieldDefaulted (tif, TIFFTAG_PLANARCONFIG, &planarconfig);
  TIFFGetFieldDefaulted (tif, TIFFTAG_ORIENTATION, &orientation);
  TIFFGetFieldDefaulted (tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetFieldDefaulted (tif, TIFFTAG_IMAGELENGTH, &height);

  if (samples_per_pixel == 4)
    {
      guint16 extra;
      guint16 *extra_types;

      if (!TIFFGetField (tif, TIFFTAG_EXTRASAMPLES, &extra, &extra_types))
        extra = 0;

      if (extra == 0 || extra_types[0] != EXTRASAMPLE_ASSOCALPHA)
        {
          g_set_error_literal (error,
                               G_IO_ERROR, G_IO_ERROR_FAILED,
                               "Did not find the alpha channel");
          TIFFClose (tif);
          return NULL;
        }
    }

  format = 0;

  for (int i = 0; i < G_N_ELEMENTS (format_data); i++)
    {
      if (format_data[i].sample_format == sample_format &&
          format_data[i].bits_per_sample == bits_per_sample &&
          format_data[i].samples_per_pixel == samples_per_pixel)
        {
          format = format_data[i].format;
          break;
        }
    }

  if (format == 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Format %s/%d-bit/%s not handled",
                   samples_per_pixel == 3 ? "RGB" : "RGBA",
                   bits_per_sample,
                   sample_format == SAMPLEFORMAT_UINT ? "int" : "float");
      TIFFClose (tif);
      return NULL;
    }

  if (photometric != PHOTOMETRIC_RGB)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Photometric %d not handled", photometric);
      TIFFClose (tif);
      return NULL;
    }

  if (planarconfig != PLANARCONFIG_CONTIG ||
      TIFFIsTiled (tif))
    {
      g_set_error_literal (error,
                           G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Non-contiguous or tiled tiff not handled");
      TIFFClose (tif);
      return NULL;
    }

  if (orientation != ORIENTATION_TOPLEFT)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Orientation %d not handled", orientation);
      TIFFClose (tif);
      return NULL;
    }

  stride = width * gdk_memory_format_bytes_per_pixel (format);

  g_assert (TIFFScanlineSize (tif) == stride);

  data = g_new (guchar, height * stride);

  line = data;
  for (int y = 0; y < height; y++)
    {
      if (TIFFReadScanline (tif, line, y, 0) == -1)
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Reading data failed at row %d", y);
          TIFFClose (tif);
          g_free (data);
          return NULL;
        }

      line += stride;
    }

  bpp = gdk_memory_format_bytes_per_pixel (format);
  bytes = g_bytes_new_take (data, width * height * bpp);

  texture = gdk_memory_texture_new (width, height,
                                    format,
                                    bytes, width * bpp);
  g_bytes_unref (bytes);

  return texture;
}

/* }}} */
/* {{{ Async code */

static void
load_tiff_in_thread (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  GInputStream *stream = source_object;
  GdkTexture *texture;
  GError *error = NULL;

  texture = gdk_load_tiff (stream, &error);

  if (texture)
    g_task_return_pointer (task, texture, g_object_unref);
  else
    g_task_return_error (task, error);
}

void
gdk_load_tiff_async (GInputStream         *stream,
                     GCancellable         *cancellable,
                     GAsyncReadyCallback   callback,
                     gpointer              user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_run_in_thread (task, load_tiff_in_thread);
  g_object_unref (task);
}

GdkTexture *
gdk_load_tiff_finish (GAsyncResult  *result,
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
} SaveTiffData;

static void
save_tiff_in_thread (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  GOutputStream *stream = source_object;
  SaveTiffData *data = task_data;
  GError *error = NULL;
  gboolean result;

  result = gdk_save_tiff (stream,
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
gdk_save_tiff_async (GOutputStream          *stream,
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
  SaveTiffData *save_data;

  save_data = g_new0 (SaveTiffData, 1);
  save_data->data = data;
  save_data->width = width;
  save_data->height = height;
  save_data->stride = stride;
  save_data->format = format;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_task_data (task, save_data, g_free);
  g_task_run_in_thread (task, save_tiff_in_thread);
  g_object_unref (task);
}

gboolean
gdk_save_tiff_finish (GAsyncResult  *result,
                      GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */

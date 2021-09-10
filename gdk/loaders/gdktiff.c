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

#include "gdktiffprivate.h"

#include "gdktexture.h"
#include "gdktextureprivate.h"
#include "gdkmemorytextureprivate.h"
#include <tiffio.h>

/* Our main interest in tiff as an image format is that it is
 * flexible enough to save all our texture formats without
 * lossy conversions.
 *
 * The loader isn't meant to be a very versatile. It just aims
 * to load the subset that we're saving ourselves. For anything
 * else, we fall back to TIFFRGBAImage, which is the same api
 * that gdk-pixbuf uses.
 */

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

/* }}} */
/* {{{ Public API */

static struct {
  GdkMemoryFormat format;
  guint16 bits_per_sample;
  guint16 samples_per_pixel;
  guint16 sample_format;
} format_data[] = {
  { GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,            8, 4, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R8G8B8,                            8, 3, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R16G16B16,                        16, 3, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,       16, 4, SAMPLEFORMAT_UINT   },
  { GDK_MEMORY_R16G16B16_FLOAT,                  16, 3, SAMPLEFORMAT_IEEEFP },
  { GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED, 16, 4, SAMPLEFORMAT_IEEEFP },
  { GDK_MEMORY_R32G32B32_FLOAT,                  32, 3, SAMPLEFORMAT_IEEEFP },
  { GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED, 32, 4, SAMPLEFORMAT_IEEEFP },
};

GBytes *
gdk_save_tiff (GdkTexture *texture)
{
  GOutputStream *stream;
  TIFF *tif;
  int width, height, stride;
  guint16 bits_per_sample = 0;
  guint16 samples_per_pixel = 0;
  guint16 sample_format = 0;
  const guchar *line;
  const guchar *data;
  guchar *new_data = NULL;
  GBytes *bytes;
  GdkTexture *memory_texture;
  GdkMemoryFormat format;

  stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  tif = tiff_open (stream, "w", NULL);
  if (!tif)
    {
      g_object_unref (stream);
      return NULL;
    }

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  memory_texture = gdk_texture_download_texture (texture);
  format = gdk_memory_texture_get_format (GDK_MEMORY_TEXTURE (memory_texture));

  for (int i = 0; i < G_N_ELEMENTS (format_data); i++)
    {
      if (format == format_data[i].format)
        {
          data = gdk_memory_texture_get_data (GDK_MEMORY_TEXTURE (memory_texture));
          stride = gdk_memory_texture_get_stride (GDK_MEMORY_TEXTURE (memory_texture));
          bits_per_sample = format_data[i].bits_per_sample;
          samples_per_pixel = format_data[i].samples_per_pixel;
          sample_format = format_data[i].sample_format;
          break;
        }
    }

  if (bits_per_sample == 0)
    {
      /* An 8-bit format we don't have in the table, handle
       * it by converting to R8G8B8A8_PREMULTIPLIED
       */
      stride = width * 4;
      new_data = g_malloc (stride * height);
      gdk_texture_download (memory_texture, new_data, stride);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      flip_02 (new_data, width, height, stride);
#endif
      data = new_data;
      bits_per_sample = 8;
      samples_per_pixel = 4;
      sample_format = SAMPLEFORMAT_UINT;
    }

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
          TIFFClose (tif);
          g_free (new_data);
          g_object_unref (memory_texture);
          g_object_unref (stream);
          return NULL;
        }

      line += stride;
    }

  TIFFFlushData (tif);
  TIFFClose (tif);

  g_free (new_data);
  g_object_unref (memory_texture);

  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (stream));
  g_object_unref (stream);

  return bytes;
}

static GdkTexture *
load_fallback (TIFF    *tif,
               GError **error)
{
  int width, height;
  guchar *data;
  GBytes *bytes;
  GdkTexture *texture;

  TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &height);

  data = g_malloc (width * height * 4);

  if (!TIFFReadRGBAImageOriented (tif, width, height, (uint32 *)data, ORIENTATION_TOPLEFT, 1))
    {
      g_set_error_literal (error,
                           G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Failed to load RGB data from TIFF file");
      g_free (data);
      return NULL;
    }

  bytes = g_bytes_new_take (data, width * height * 4);

  texture = gdk_memory_texture_new (width, height,
                                    GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                                    bytes,
                                    width * 4);

  g_bytes_unref (bytes);

  return texture;
}

GdkTexture *
gdk_load_tiff (GBytes  *input_bytes,
               GError **error)
{
  GInputStream *stream;
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

  stream = g_memory_input_stream_new_from_bytes (input_bytes);
  tif = tiff_open (stream, "r", error);
  g_object_unref (stream);

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
          texture = load_fallback (tif, error);
          TIFFClose (tif);
          return texture;
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

  if (format == 0 ||
      photometric != PHOTOMETRIC_RGB ||
      planarconfig != PLANARCONFIG_CONTIG ||
      TIFFIsTiled (tif) ||
      orientation != ORIENTATION_TOPLEFT)
    {
      texture = load_fallback (tif, error);
      TIFFClose (tif);
      return texture;
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

  TIFFClose (tif);

  return texture;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */

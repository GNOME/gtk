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

#include "gdkjpeg.h"

#include "gdktexture.h"
#include "gdkmemorytextureprivate.h"
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

/* {{{ IO handling */

#define BUF_SIZE 65536

typedef struct {
  struct jpeg_source_mgr pub;
  JOCTET buffer[BUF_SIZE];
  long skip_next;
  GInputStream *stream;
} my_source_mgr;

static void
init_source (j_decompress_ptr info)
{
  my_source_mgr *src = (my_source_mgr *) info->src;
  src->skip_next = 0;
}

static void
term_source (j_decompress_ptr info)
{
}

static boolean
fill_input_buffer (j_decompress_ptr info)
{
  my_source_mgr *src = (my_source_mgr *) info->src;
  size_t nbytes;

  nbytes = g_input_stream_read (src->stream, src->buffer, BUF_SIZE, NULL, NULL);

  if (nbytes <= 0)
    {
      /* Insert a fake EOI marker */
      src->buffer[0] = (JOCTET) 0xFF;
      src->buffer[1] = (JOCTET) JPEG_EOI;
      nbytes = 2;
    }

  src->pub.next_input_byte = src->buffer;
  src->pub.bytes_in_buffer = nbytes;

  return TRUE;
}

static void
skip_input_data (j_decompress_ptr info,
                 long             num_bytes)
{
  my_source_mgr *src = (my_source_mgr *) info->src;

  if (num_bytes > 0)
    {
      while (num_bytes > src->pub.bytes_in_buffer)
        {
          num_bytes -= src->pub.bytes_in_buffer;
          fill_input_buffer (info);
        }

      src->pub.next_input_byte += num_bytes;
      src->pub.bytes_in_buffer -= num_bytes;
    }
}

/* }}} */
/* {{{ Error handling */

struct error_handler_data {
        struct jpeg_error_mgr pub;
        sigjmp_buf setjmp_buffer;
        GError **error;
};

static void
fatal_error_handler (j_common_ptr cinfo)
{
  struct error_handler_data *errmgr;

  errmgr = (struct error_handler_data *) cinfo->err;

  siglongjmp (errmgr->setjmp_buffer, 1);

  g_assert_not_reached ();
}

static void
output_message_handler (j_common_ptr cinfo)
{
  /* do nothing */
}

/* }}} */
 /* {{{ Public API */

GdkTexture *
gdk_load_jpeg (GInputStream  *stream,
               GError       **error)
{
  struct jpeg_decompress_struct info;
  struct error_handler_data jerr;
  struct jpeg_error_mgr err;
  my_source_mgr src;
  int width, height;
  int size;
  unsigned char *data;
  unsigned char *row[1];
  GBytes *bytes;
  GdkTexture *texture;

  info.err = jpeg_std_error (&jerr.pub);
  jerr.pub.error_exit = fatal_error_handler;
  jerr.pub.output_message = output_message_handler;
  jerr.error = error;

  if (sigsetjmp (jerr.setjmp_buffer, 1))
    {
      jpeg_destroy_decompress (&info);
      return NULL;
    }

  info.err = jpeg_std_error (&err);
  jpeg_create_decompress (&info);

  src.pub.init_source = init_source;
  src.pub.fill_input_buffer = fill_input_buffer;
  src.pub.skip_input_data = skip_input_data;
  src.pub.resync_to_restart = jpeg_resync_to_restart;
  src.pub.term_source = term_source;
  src.pub.bytes_in_buffer = 0;
  src.pub.next_input_byte = NULL;
  src.stream = stream;

  info.src = (struct jpeg_source_mgr *)&src;

  jpeg_read_header (&info, TRUE);
  jpeg_start_decompress (&info);

  width = info.output_width;
  height = info.output_height;

  size = width * height * 3;
  data = g_malloc (size);

  while (info.output_scanline < info.output_height)
    {
       row[0] = (unsigned char *)(&data[3 *info.output_width * info.output_scanline]);
       jpeg_read_scanlines (&info, row, 1);
    }

  jpeg_finish_decompress (&info);
  jpeg_destroy_decompress (&info);

  bytes = g_bytes_new_take (data, size);

  texture = gdk_memory_texture_new (width, height,
                                    GDK_MEMORY_R8G8B8,
                                    bytes, width * 3);

  g_bytes_unref (bytes);

  return texture;
}

/* }}} */
/* {{{ Async code */

static void
load_jpeg_in_thread (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  GInputStream *stream = source_object;
  GdkTexture *texture;
  GError *error = NULL;

  texture = gdk_load_jpeg (stream, &error);

  if (texture)
    g_task_return_pointer (task, texture, g_object_unref);
  else
    g_task_return_error (task, error);
}

void
gdk_load_jpeg_async (GInputStream         *stream,
                     GCancellable         *cancellable,
                     GAsyncReadyCallback   callback,
                     gpointer              user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_run_in_thread (task, load_jpeg_in_thread);
  g_object_unref (task);
}

GdkTexture *
gdk_load_jpeg_finish (GAsyncResult  *result,
                      GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */

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

#include "gdkjpegprivate.h"

#include "gdktexture.h"
#include "gdkmemorytextureprivate.h"
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

/* {{{ Error handling */

/* No sigsetjmp on Windows */
#ifndef HAVE_SIGSETJMP
#define sigjmp_buf jmp_buf
#define sigsetjmp(jb, x) setjmp(jb)
#define siglongjmp longjmp
#endif

struct error_handler_data {
        struct jpeg_error_mgr pub;
        sigjmp_buf setjmp_buffer;
        GError **error;
};

static void
fatal_error_handler (j_common_ptr cinfo)
{
  struct error_handler_data *errmgr;
  char buffer[JMSG_LENGTH_MAX];

  errmgr = (struct error_handler_data *) cinfo->err;

  cinfo->err->format_message (cinfo, buffer);

  if (errmgr->error && *errmgr->error == NULL)
    g_set_error (errmgr->error,
                 GDK_TEXTURE_ERROR,
                 cinfo->err->msg_code == JERR_OUT_OF_MEMORY
                   ? GDK_TEXTURE_ERROR_INSUFFICIENT_MEMORY
                   : GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                 "Error interpreting JPEG image file (%s)", buffer);

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
gdk_load_jpeg (GBytes  *input_bytes,
               GError **error)
{
  struct jpeg_decompress_struct info;
  struct error_handler_data jerr;
  struct jpeg_error_mgr err;
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

  jpeg_mem_src (&info,
                g_bytes_get_data (input_bytes, NULL),
                g_bytes_get_size (input_bytes));

  jpeg_read_header (&info, TRUE);
  jpeg_start_decompress (&info);

  width = info.output_width;
  height = info.output_height;

  size = width * height * 3;
  data = g_try_malloc_n (width * 3, height);
  if (!data)
    {
      g_set_error_literal (error,
                           GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_INSUFFICIENT_MEMORY,
                           "Not enough memory to load jpeg");
      jpeg_destroy_decompress (&info);
      return NULL;
    }

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

/* vim:set foldmethod=marker expandtab: */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <cairo.h>

#include "broadway-output.h"

/************************************************************************
 *                Basic I/O primitives                                  *
 ************************************************************************/

struct BroadwayOutput {
  GOutputStream *out;
  GOutputStream *buf;
  int error;
  guint32 serial;
  int compression;
};

#define SEND_CMD_RESERVE_SIZE	16

static void
broadway_output_send_cmd (BroadwayOutput *output,
			  gboolean fin, BroadwayWSOpCode code,
			  void *reservation_and_data, gsize count)
{
  gboolean mask = FALSE;
  guchar header[SEND_CMD_RESERVE_SIZE];
  guchar *ptr;
  size_t header_size;

  gboolean mid_header = count > 125 && count <= 65535;
  gboolean long_header = count > 65535;

  /* NB. big-endian spec => bit 0 == MSB */
  header[0] = ( (fin ? 0x80 : 0) | (code & 0x0f) );
  header[1] = ( (mask ? 0x80 : 0) |
                (mid_header ? 126 : long_header ? 127 : count) );
  header_size = 2;
  if (mid_header)
    {
      *(guint16 *)(header + header_size) = GUINT16_TO_BE( (guint16)count );
      header_size += 2;
    }
  else if (long_header)
    {
      *(guint64 *)(header + header_size) = GUINT64_TO_BE( count );
      header_size += 8;
    }
  if (reservation_and_data && count)
    {
      ptr = (guchar *)reservation_and_data;
      ptr+= (SEND_CMD_RESERVE_SIZE - header_size);
      memcpy(ptr, header, header_size);
      g_output_stream_write_all (output->out, ptr, count + header_size, NULL, NULL, NULL);
    }
  else
    {
      g_output_stream_write_all (output->out, header, header_size, NULL, NULL, NULL);
    }
}

void broadway_output_pong (BroadwayOutput *output)
{
  broadway_output_send_cmd (output, TRUE, BROADWAY_WS_CNX_PONG, NULL, 0);
}

int
broadway_output_flush (BroadwayOutput *output)
{
  GError *error;
  gpointer data;
  gsize len;

  len = g_seekable_tell(G_SEEKABLE (output->buf));
  if (len == SEND_CMD_RESERVE_SIZE)
    return TRUE;

  assert( len > SEND_CMD_RESERVE_SIZE);

  data = g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM (output->buf) );

  broadway_output_send_cmd (output, TRUE, BROADWAY_WS_BINARY, data, len - SEND_CMD_RESERVE_SIZE);
  if (output->error)
      return FALSE;

  g_seekable_seek(G_SEEKABLE(output->buf), 
    SEND_CMD_RESERVE_SIZE, G_SEEK_SET, NULL, &error);
  return TRUE;
}

BroadwayOutput *
broadway_output_new (GOutputStream *out, guint32 serial, int compression)
{
  BroadwayOutput *output;
  char reservation[SEND_CMD_RESERVE_SIZE] = { 0 };
  if (compression > 9)
      compression = 9;
  if (compression < -1)
      compression = -1;
  output = g_new0 (BroadwayOutput, 1);

  output->out = g_object_ref (out);
  output->buf = g_memory_output_stream_new_resizable ();
  output->serial = serial;
  output->compression = compression;
  g_output_stream_write_all (output->buf, &reservation, 
      SEND_CMD_RESERVE_SIZE, NULL, NULL, NULL);

  return output;
}

void
broadway_output_free (BroadwayOutput *output)
{
  g_object_unref (output->out);
  g_object_unref (output->buf);
  free (output);
}

guint32
broadway_output_get_next_serial (BroadwayOutput *output)
{
  return output->serial;
}

void
broadway_output_set_next_serial (BroadwayOutput *output,
				 guint32 serial)
{
  output->serial = serial;
}


/************************************************************************
 *                     Core rendering operations                        *
 ************************************************************************/

static inline void
append_char (BroadwayOutput *output, char c)
{
  g_output_stream_write_all (output->buf, &c, sizeof (c), NULL, NULL, NULL);
}

static inline void
append_bool (BroadwayOutput *output, gboolean val)
{
  const unsigned char tmp = val ? 1 : 0;
  g_output_stream_write_all (output->buf, &tmp, sizeof (tmp), NULL, NULL, NULL);
}

static inline void
append_flags (BroadwayOutput *output, guint32 val)
{
  const unsigned char tmp = (unsigned char)val;
  g_output_stream_write_all (output->buf, &tmp, sizeof (tmp), NULL, NULL, NULL);
}

static void
append_uint16 (BroadwayOutput *output, guint32 v)
{
  const unsigned char tmp[2] = { (v >> 0) & 0xff, (v >> 8) & 0xff };
  g_output_stream_write_all (output->buf, &tmp, sizeof (tmp), NULL, NULL, NULL);
}

static void
append_uint32 (BroadwayOutput *output, guint32 v)
{
  const unsigned char tmp[4] = { (v >> 0) & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff };
  g_output_stream_write_all (output->buf, &tmp, sizeof (tmp), NULL, NULL, NULL);
}

static void
write_header(BroadwayOutput *output, char op)
{
  append_char (output, op);
  append_uint32 (output, output->serial++);
}

void
broadway_output_grab_pointer (BroadwayOutput *output,
			      int id,
			      gboolean owner_event)
{
  write_header (output, BROADWAY_OP_GRAB_POINTER);
  append_uint16 (output, id);
  append_bool (output, owner_event);
}

guint32
broadway_output_ungrab_pointer (BroadwayOutput *output)
{
  guint32 serial;

  serial = output->serial;
  write_header (output, BROADWAY_OP_UNGRAB_POINTER);

  return serial;
}

void
broadway_output_new_surface(BroadwayOutput *output,
			    int id, int x, int y, int w, int h,
			    gboolean is_temp)
{
  write_header (output, BROADWAY_OP_NEW_SURFACE);
  append_uint16 (output, id);
  append_uint16 (output, x);
  append_uint16 (output, y);
  append_uint16 (output, w);
  append_uint16 (output, h);
  append_bool (output, is_temp);
}

void
broadway_output_disconnected (BroadwayOutput *output)
{
  write_header (output, BROADWAY_OP_DISCONNECTED);
}

void
broadway_output_show_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_SHOW_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_hide_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_HIDE_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_raise_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_RAISE_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_lower_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_LOWER_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_destroy_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_DESTROY_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_set_show_keyboard (BroadwayOutput *output,
                                   gboolean show)
{
  write_header (output, BROADWAY_OP_SET_SHOW_KEYBOARD);
  append_uint16 (output, show);
}

void
broadway_output_move_resize_surface (BroadwayOutput *output,
				     int             id,
				     gboolean        has_pos,
				     int             x,
				     int             y,
				     gboolean        has_size,
				     int             w,
				     int             h)
{
  int val;

  if (!has_pos && !has_size)
    return;

  write_header (output, BROADWAY_OP_MOVE_RESIZE);
  val = (!!has_pos) | ((!!has_size) << 1);
  append_uint16 (output, id);
  append_flags (output, val);
  if (has_pos)
    {
      append_uint16 (output, x);
      append_uint16 (output, y);
    }
  if (has_size)
    {
      append_uint16 (output, w);
      append_uint16 (output, h);
    }
}

void
broadway_output_set_transient_for (BroadwayOutput *output,
				   int             id,
				   int             parent_id)
{
  write_header (output, BROADWAY_OP_SET_TRANSIENT_FOR);
  append_uint16 (output, id);
  append_uint16 (output, parent_id);
}

void
broadway_output_put_buffer (BroadwayOutput *output,
                            int             id,
                            BroadwayBuffer *prev_buffer,
                            BroadwayBuffer *buffer)
{
  guint32 len;
  unsigned char *plen;
  goffset where_is_len;
  GZlibCompressor *compressor;
  GOutputStream *out;
  GError *error;
  gpointer data;
  int w, h;

  write_header (output, (output->compression == 0) 
      ? BROADWAY_OP_PUT_UNCOMPRESSED_BUFFER : BROADWAY_OP_PUT_BUFFER);

  w = broadway_buffer_get_width (buffer);
  h = broadway_buffer_get_height (buffer);

  append_uint16 (output, id);
  append_uint16 (output, w);
  append_uint16 (output, h);

  //remember where to seek and write later when len will be known, put now dummy 32 bits here
  where_is_len = g_seekable_tell(G_SEEKABLE (output->buf));
  len = 0xdeadbeef;
  g_output_stream_write_all (output->buf, &len, sizeof(len), NULL, NULL, NULL);

  if (output->compression == 0)
    {
      broadway_buffer_encode (buffer, prev_buffer, output->buf);
    }
  else
    {
      compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_RAW, output->compression);
      if (!compressor)
          return;

      out = g_converter_output_stream_new (output->buf, G_CONVERTER (compressor));
      g_object_unref (compressor);
      if (!out)
          return;

      broadway_buffer_encode (buffer, prev_buffer, out);
      g_filter_output_stream_set_close_base_stream(G_FILTER_OUTPUT_STREAM(out), FALSE);
      if (!g_output_stream_close (out, NULL, &error))
          g_warning ("compression failed");

      g_object_unref (out);
    }
  len = (guint32) (g_seekable_tell(G_SEEKABLE (output->buf)) - (where_is_len + sizeof(len)));

  //now we know actual len value, put it where it should be
  data = g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM (output->buf) );
  plen = (unsigned char *)data + where_is_len;
  plen[0] = len & 0xff;
  plen[1] = (len >> 8) & 0xff;
  plen[2] = (len >> 16) & 0xff;
  plen[3] = (len >> 24) & 0xff;
}

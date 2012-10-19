/* GDK - The GIMP Drawing Kit
 * gdkdisplay-broadway.c
 * 
 * Copyright 2001 Sun Microsystems Inc.
 * Copyright (C) 2004 Nokia Corporation
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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

#include "gdkdisplay-broadway.h"

#include "gdkdisplay.h"
#include "gdkeventsource.h"
#include "gdkscreen.h"
#include "gdkscreen-broadway.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager-broadway.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

static void   gdk_broadway_display_dispose            (GObject            *object);
static void   gdk_broadway_display_finalize           (GObject            *object);

#if 0
#define DEBUG_WEBSOCKETS 1
#endif

G_DEFINE_TYPE (GdkBroadwayDisplay, gdk_broadway_display, GDK_TYPE_DISPLAY)

static void
gdk_broadway_display_init (GdkBroadwayDisplay *display)
{
  _gdk_broadway_display_manager_add_display (gdk_display_manager_get (),
					     GDK_DISPLAY_OBJECT (display));
  display->id_ht = g_hash_table_new (NULL, NULL);
}

static void
gdk_event_init (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (display);
  broadway_display->event_source = _gdk_broadway_event_source_new (display);
  broadway_display->saved_serial = 1;
  broadway_display->last_seen_time = 1;
}

static void
gdk_broadway_display_init_input (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  GList *list, *l;

  broadway_display = GDK_BROADWAY_DISPLAY (display);
  device_manager = gdk_display_get_device_manager (display);

  /* For backwards compatibility, just add
   * floating devices that are not keyboards.
   */
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING);

  for (l = list; l; l = l->next)
    {
      device = l->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
        continue;

      broadway_display->input_devices = g_list_prepend (broadway_display->input_devices,
                                                   g_object_ref (l->data));
    }

  g_list_free (list);

  /* Now set "core" pointer to the first
   * master device that is a pointer.
   */
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (l = list; l; l = l->next)
    {
      device = list->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
        continue;

      display->core_pointer = device;
      break;
    }

  /* Add the core pointer to the devices list */
  broadway_display->input_devices = g_list_prepend (broadway_display->input_devices,
                                               g_object_ref (display->core_pointer));

  g_list_free (list);
}

typedef struct HttpRequest {
  GdkDisplay *display;
  GSocketConnection *connection;
  GDataInputStream *data;
  GString *request;
}  HttpRequest;

static void start_output (HttpRequest *request, gboolean proto_v7_plus, gboolean binary);

static void
http_request_free (HttpRequest *request)
{
  g_object_unref (request->connection);
  g_object_unref (request->data);
  g_string_free (request->request, TRUE);
  g_free (request);
}

struct BroadwayInput {
  GdkDisplay *display;
  GSocketConnection *connection;
  GByteArray *buffer;
  GSource *source;
  gboolean seen_time;
  gint64 time_base;
  gboolean proto_v7_plus;
  gboolean binary;
};

static void
broadway_input_free (BroadwayInput *input)
{
  g_object_unref (input->connection);
  g_byte_array_free (input->buffer, FALSE);
  g_source_destroy (input->source);
  g_free (input);
}

static void
process_input_messages (GdkBroadwayDisplay *broadway_display)
{
  BroadwayInputMsg *message;

  while (broadway_display->input_messages)
    {
      message = broadway_display->input_messages->data;
      broadway_display->input_messages =
	g_list_delete_link (broadway_display->input_messages,
			    broadway_display->input_messages);

      _gdk_broadway_events_got_input (GDK_DISPLAY (broadway_display), message);
      g_free (message);
    }
}

static char *
parse_pointer_data (char *p, BroadwayInputPointerMsg *data)
{
  data->mouse_window_id = strtol (p, &p, 10);
  p++; /* Skip , */
  data->event_window_id = strtol (p, &p, 10);
  p++; /* Skip , */
  data->root_x = strtol (p, &p, 10);
  p++; /* Skip , */
  data->root_y = strtol (p, &p, 10);
  p++; /* Skip , */
  data->win_x = strtol (p, &p, 10);
  p++; /* Skip , */
  data->win_y = strtol (p, &p, 10);
  p++; /* Skip , */
  data->state = strtol (p, &p, 10);

  return p;
}

static void
update_future_pointer_info (GdkBroadwayDisplay *broadway_display, BroadwayInputPointerMsg *data)
{
  broadway_display->future_root_x = data->root_x;
  broadway_display->future_root_y = data->root_y;
  broadway_display->future_state = data->state;
  broadway_display->future_mouse_in_toplevel = data->mouse_window_id;
}

static void
parse_input_message (BroadwayInput *input, const char *message)
{
  GdkBroadwayDisplay *broadway_display;
  BroadwayInputMsg msg;
  char *p;
  gint64 time_;

  broadway_display = GDK_BROADWAY_DISPLAY (input->display);

  p = (char *)message;
  msg.base.type = *p++;
  msg.base.serial = (guint32)strtol (p, &p, 10);
  p++; /* Skip , */
  time_ = strtol(p, &p, 10);
  p++; /* Skip , */

  if (time_ == 0) {
    time_ = broadway_display->last_seen_time;
  } else {
    if (!input->seen_time) {
      input->seen_time = TRUE;
      /* Calculate time base so that any following times are normalized to start
	 5 seconds after last_seen_time, to avoid issues that could appear when
	 a long hiatus due to a reconnect seems to be instant */
      input->time_base = time_ - (broadway_display->last_seen_time + 5000);
    }
    time_ = time_ - input->time_base;
  }

  broadway_display->last_seen_time = time_;

  msg.base.time = time_;

  switch (msg.base.type) {
  case 'e': /* Enter */
  case 'l': /* Leave */
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (broadway_display, &msg.pointer);
    p++; /* Skip , */
    msg.crossing.mode = strtol(p, &p, 10);
    break;

  case 'm': /* Mouse move */
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (broadway_display, &msg.pointer);
    break;

  case 'b':
  case 'B':
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (broadway_display, &msg.pointer);
    p++; /* Skip , */
    msg.button.button = strtol(p, &p, 10);
    break;

  case 's':
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (broadway_display, &msg.pointer);
    p++; /* Skip , */
    msg.scroll.dir = strtol(p, &p, 10);
    break;

  case 'k':
  case 'K':
    msg.key.key = strtol(p, &p, 10);
    p++; /* Skip , */
    msg.key.state = strtol(p, &p, 10);
    break;

  case 'g':
  case 'u':
    msg.grab_reply.res = strtol(p, &p, 10);
    break;

  case 'w':
    msg.configure_notify.id = strtol(p, &p, 10);
    p++; /* Skip , */
    msg.configure_notify.x = strtol (p, &p, 10);
    p++; /* Skip , */
    msg.configure_notify.y = strtol (p, &p, 10);
    p++; /* Skip , */
    msg.configure_notify.width = strtol (p, &p, 10);
    p++; /* Skip , */
    msg.configure_notify.height = strtol (p, &p, 10);
    break;

  case 'W':
    msg.delete_notify.id = strtol(p, &p, 10);
    break;

  case 'd':
    msg.screen_resize_notify.width = strtol (p, &p, 10);
    p++; /* Skip , */
    msg.screen_resize_notify.height = strtol (p, &p, 10);
    break;

  default:
    g_printerr ("Unknown input command %s\n", message);
    break;
  }

  broadway_display->input_messages = g_list_append (broadway_display->input_messages, g_memdup (&msg, sizeof (msg)));

}

static inline void
hex_dump (guchar *data, gsize len)
{
#ifdef DEBUG_WEBSOCKETS
  gsize i, j;
  for (j = 0; j < len + 15; j += 16)
    {
      fprintf (stderr, "0x%.4x  ", j);
      for (i = 0; i < 16; i++)
	{
	    if ((j + i) < len)
	      fprintf (stderr, "%.2x ", data[j+i]);
	    else
	      fprintf (stderr, "  ");
	    if (i == 8)
	      fprintf (stderr, " ");
	}
      fprintf (stderr, " | ");

      for (i = 0; i < 16; i++)
	if ((j + i) < len && g_ascii_isalnum(data[j+i]))
	  fprintf (stderr, "%c", data[j+i]);
	else
	  fprintf (stderr, ".");
      fprintf (stderr, "\n");
    }
#endif
}

static void
parse_input (BroadwayInput *input)
{
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (input->display);

  if (!input->buffer->len)
    return;

  if (input->proto_v7_plus)
    {
      hex_dump (input->buffer->data, input->buffer->len);

      while (input->buffer->len > 2)
	{
	  gsize len, payload_len;
	  BroadwayWSOpCode code;
	  gboolean is_mask, fin;
	  guchar *buf, *data, *mask;

	  buf = input->buffer->data;
	  len = input->buffer->len;

#ifdef DEBUG_WEBSOCKETS
	  g_print ("Parse input first byte 0x%2x 0x%2x\n", buf[0], buf[1]);
#endif

	  fin = buf[0] & 0x80;
	  code = buf[0] & 0x0f;
	  payload_len = buf[1] & 0x7f;
	  is_mask = buf[1] & 0x80;
	  data = buf + 2;

	  if (payload_len > 125)
	    {
	      if (len < 4)
		return;
	      payload_len = GUINT16_FROM_BE( *(guint16 *) data );
	      data += 2;
	    }
	  else if (payload_len > 126)
	    {
	      if (len < 10)
		return;
	      payload_len = GUINT64_FROM_BE( *(guint64 *) data );
	      data += 8;
	    }

	  mask = NULL;
	  if (is_mask)
	    {
	      if (data - buf + 4 > len)
		return;
	      mask = data;
	      data += 4;
	    }

	  if (data - buf + payload_len > len)
	    return; /* wait to accumulate more */

	  if (is_mask)
	    {
	      gsize i;
	      for (i = 0; i < payload_len; i++)
		data[i] ^= mask[i%4];
	    }

	  switch (code) {
	  case BROADWAY_WS_CNX_CLOSE:
	    break; /* hang around anyway */
	  case BROADWAY_WS_TEXT:
	    if (!fin)
	      {
#ifdef DEBUG_WEBSOCKETS
		g_warning ("can't yet accept fragmented input");
#endif
	      }
	    else
	      {
		char *terminated = g_strndup((char *)data, payload_len);
	        parse_input_message (input, terminated);
		g_free (terminated);
	      }
	    break;
	  case BROADWAY_WS_CNX_PING:
	    broadway_output_pong (broadway_display->output);
	    break;
	  case BROADWAY_WS_CNX_PONG:
	    break; /* we never send pings, but tolerate pongs */
	  case BROADWAY_WS_BINARY:
	  case BROADWAY_WS_CONTINUATION:
	  default:
	    {
	      g_warning ("fragmented or unknown input code 0x%2x with fin set", code);
	      break;
	    }
	  }

	  g_byte_array_remove_range (input->buffer, 0, data - buf + payload_len);
	}
    }
  else /* old style protocol */
    {
      char *buf, *ptr;
      gsize len;

      buf = (char *)input->buffer->data;
      len = input->buffer->len;

      if (buf[0] != 0)
	{
	  broadway_display->input = NULL;
	  broadway_input_free (input);
	  return;
	}

      while ((ptr = memchr (buf, 0xff, len)) != NULL)
	{
	  *ptr = 0;
	  ptr++;

	  parse_input_message (input, buf + 1);

	  len -= ptr - buf;
	  buf = ptr;

	  if (len > 0 && buf[0] != 0)
	    {
	      broadway_display->input = NULL;
	      broadway_input_free (input);
	      break;
	    }
	}
      g_byte_array_remove_range (input->buffer, 0, buf - (char *)input->buffer->data);
    }
}


static gboolean
process_input_idle_cb (GdkBroadwayDisplay *display)
{
  display->process_input_idle = 0;
  process_input_messages (display);
  return G_SOURCE_REMOVE;
}

static void
queue_process_input_at_idle (GdkBroadwayDisplay *broadway_display)
{
  if (broadway_display->process_input_idle == 0)
    broadway_display->process_input_idle =
      g_idle_add_full (GDK_PRIORITY_EVENTS, (GSourceFunc)process_input_idle_cb, broadway_display, NULL);
}

static void
_gdk_broadway_display_read_all_input_nonblocking (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;
  GInputStream *in;
  gssize res;
  guint8 buffer[1024];
  GError *error;
  BroadwayInput *input;

  broadway_display = GDK_BROADWAY_DISPLAY (display);
  if (broadway_display->input == NULL)
    return;

  input = broadway_display->input;

  in = g_io_stream_get_input_stream (G_IO_STREAM (input->connection));

  error = NULL;
  res = g_pollable_input_stream_read_nonblocking (G_POLLABLE_INPUT_STREAM (in),
						  buffer, sizeof (buffer), NULL, &error);

  if (res <= 0)
    {
      if (res < 0 &&
	  g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
	{
	  g_error_free (error);
	  return;
	}

      broadway_display->input = NULL;
      broadway_input_free (input);
      if (res < 0)
	{
	  g_print ("input error %s\n", error->message);
	  g_error_free (error);
	}
      return;
    }

  g_byte_array_append (input->buffer, buffer, res);

  parse_input (input);
}

void
_gdk_broadway_display_consume_all_input (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (display);
  _gdk_broadway_display_read_all_input_nonblocking (display);

  /* Since we're parsing input but not processing the resulting messages
     we might not get a readable callback on the stream, so queue an idle to
     process the messages */
  queue_process_input_at_idle (broadway_display);
}


static gboolean
input_data_cb (GObject  *stream,
	       BroadwayInput *input)
{
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (input->display);
  _gdk_broadway_display_read_all_input_nonblocking (input->display);

  process_input_messages (broadway_display);

  return TRUE;
}

/* Note: This may be called while handling a message (i.e. sorta recursively) */
BroadwayInputMsg *
_gdk_broadway_display_block_for_input (GdkDisplay *display, char op,
				       guint32 serial, gboolean remove_message)
{
  GdkBroadwayDisplay *broadway_display;
  BroadwayInputMsg *message;
  gssize res;
  guint8 buffer[1024];
  BroadwayInput *input;
  GInputStream *in;
  GList *l;

  gdk_display_flush (display);

  broadway_display = GDK_BROADWAY_DISPLAY (display);
  if (broadway_display->input == NULL)
    return NULL;

  input = broadway_display->input;

  while (TRUE) {
    /* Check for existing reply in queue */

    for (l = broadway_display->input_messages; l != NULL; l = l->next)
      {
	message = l->data;

	if (message->base.type == op)
	  {
	    if (message->base.serial == serial)
	      {
		if (remove_message)
		  broadway_display->input_messages =
		    g_list_delete_link (broadway_display->input_messages, l);
		return message;
	      }
	  }
      }

    /* Not found, read more, blocking */

    in = g_io_stream_get_input_stream (G_IO_STREAM (input->connection));
    res = g_input_stream_read (in, buffer, sizeof (buffer), NULL, NULL);
    if (res <= 0)
      return NULL;
    g_byte_array_append (input->buffer, buffer, res);

    parse_input (input);

    /* Since we're parsing input but not processing the resulting messages
       we might not get a readable callback on the stream, so queue an idle to
       process the messages */
    queue_process_input_at_idle (broadway_display);
  }
}

static char *
parse_line (char *line, char *key)
{
  char *p;

  if (!g_str_has_prefix (line, key))
    return NULL;
  p = line + strlen (key);
  if (*p != ':')
    return NULL;
  p++;
  /* Skip optional initial space */
  if (*p == ' ')
    p++;
  return p;
}
static void
send_error (HttpRequest *request,
	    int error_code,
	    const char *reason)
{
  char *res;

  res = g_strdup_printf ("HTTP/1.0 %d %s\r\n\r\n"
			 "<html><head><title>%d %s</title></head>"
			 "<body>%s</body></html>",
			 error_code, reason,
			 error_code, reason,
			 reason);
  /* TODO: This should really be async */
  g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
			     res, strlen (res), NULL, NULL, NULL);
  g_free (res);
  http_request_free (request);
}

/* magic from: http://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-17 */
#define SEC_WEB_SOCKET_KEY_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* 'x3JJHMbDL1EzLkh9GBhXDw==' generates 'HSmrc0sMlYUkAGmm5OPpG2HaGWk=' */
static gchar *
generate_handshake_response_wsietf_v7 (const gchar *key)
{
  gsize digest_len = 20;
  guchar digest[digest_len];
  GChecksum *checksum;

  checksum = g_checksum_new (G_CHECKSUM_SHA1);
  if (!checksum)
    return NULL;

  g_checksum_update (checksum, (guchar *)key, -1);
  g_checksum_update (checksum, (guchar *)SEC_WEB_SOCKET_KEY_MAGIC, -1);

  g_checksum_get_digest (checksum, digest, &digest_len);
  g_checksum_free (checksum);

  g_assert (digest_len == 20);

  return g_base64_encode (digest, digest_len);
}

static void
start_input (HttpRequest *request, gboolean binary)
{
  char **lines;
  char *p;
  int num_key1, num_key2;
  guint64 key1, key2;
  int num_space;
  int i;
  guint8 challenge[16];
  char *res;
  gsize len;
  GChecksum *checksum;
  char *origin, *host;
  GdkBroadwayDisplay *broadway_display;
  BroadwayInput *input;
  const void *data_buffer;
  gsize data_buffer_size;
  GInputStream *in;
  char *key_v7;
  gboolean proto_v7_plus;

  broadway_display = GDK_BROADWAY_DISPLAY (request->display);

  if (broadway_display->input != NULL)
    {
      send_error (request, 409, "Input already handled");
      return;
    }

#ifdef DEBUG_WEBSOCKETS
  g_print ("incoming request:\n%s\n", request->request->str);
#endif
  lines = g_strsplit (request->request->str, "\n", 0);

  num_key1 = 0;
  num_key2 = 0;
  key1 = 0;
  key2 = 0;
  key_v7 = NULL;
  origin = NULL;
  host = NULL;
  for (i = 0; lines[i] != NULL; i++)
    {
      if ((p = parse_line (lines[i], "Sec-WebSocket-Key1")))
	{
	  num_space = 0;
	  while (*p != 0)
	    {
	      if (g_ascii_isdigit (*p))
		key1 = key1 * 10 + g_ascii_digit_value (*p);
	      else if (*p == ' ')
		num_space++;

	      p++;
	    }
	  key1 /= num_space;
	  num_key1++;
	}
      else if ((p = parse_line (lines[i], "Sec-WebSocket-Key2")))
	{
	  num_space = 0;
	  while (*p != 0)
	    {
	      if (g_ascii_isdigit (*p))
		key2 = key2 * 10 + g_ascii_digit_value (*p);
	      else if (*p == ' ')
		num_space++;

	      p++;
	    }
	  key2 /= num_space;
	  num_key2++;
	}
      else if ((p = parse_line (lines[i], "Sec-WebSocket-Key")))
	{
	  key_v7 = p;
	}
      else if ((p = parse_line (lines[i], "Origin")))
	{
	  origin = p;
	}
      else if ((p = parse_line (lines[i], "Host")))
	{
	  host = p;
	}
      else if ((p = parse_line (lines[i], "Sec-WebSocket-Origin")))
	{
	  origin = p;
	}
    }

  if (origin == NULL || host == NULL)
    {
      g_strfreev (lines);
      send_error (request, 400, "Bad websocket request");
      return;
    }

  if (key_v7 != NULL)
    {
      char* accept = generate_handshake_response_wsietf_v7 (key_v7);
      res = g_strdup_printf ("HTTP/1.1 101 Switching Protocols\r\n"
			     "Upgrade: websocket\r\n"
			     "Connection: Upgrade\r\n"
			     "Sec-WebSocket-Accept: %s\r\n"
			     "Sec-WebSocket-Origin: %s\r\n"
			     "Sec-WebSocket-Location: ws://%s/socket\r\n"
			     "Sec-WebSocket-Protocol: broadway\r\n"
			     "\r\n", accept, origin, host);
      g_free (accept);

#ifdef DEBUG_WEBSOCKETS
      g_print ("v7 proto response:\n%s", res);
#endif

      g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
				 res, strlen (res), NULL, NULL, NULL);
      g_free (res);
      proto_v7_plus = TRUE;
    }
  else
    {
      if (num_key1 != 1 || num_key2 != 1)
	{
	  g_strfreev (lines);
	  send_error (request, 400, "Bad websocket request");
	  return;
	}

      challenge[0] = (key1 >> 24) & 0xff;
      challenge[1] = (key1 >> 16) & 0xff;
      challenge[2] = (key1 >>  8) & 0xff;
      challenge[3] = (key1 >>  0) & 0xff;
      challenge[4] = (key2 >> 24) & 0xff;
      challenge[5] = (key2 >> 16) & 0xff;
      challenge[6] = (key2 >>  8) & 0xff;
      challenge[7] = (key2 >>  0) & 0xff;

      if (!g_input_stream_read_all (G_INPUT_STREAM (request->data), challenge+8, 8, NULL, NULL, NULL))
	{
	  g_strfreev (lines);
	  send_error (request, 400, "Bad websocket request");
	  return;
	}

      checksum = g_checksum_new (G_CHECKSUM_MD5);
      g_checksum_update (checksum, challenge, 16);
      len = 16;
      g_checksum_get_digest (checksum, challenge, &len);
      g_checksum_free (checksum);

      res = g_strdup_printf ("HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
			     "Upgrade: WebSocket\r\n"
			     "Connection: Upgrade\r\n"
			     "Sec-WebSocket-Origin: %s\r\n"
			     "Sec-WebSocket-Location: ws://%s/socket\r\n"
			     "Sec-WebSocket-Protocol: broadway\r\n"
			     "\r\n",
			     origin, host);

#ifdef DEBUG_WEBSOCKETS
      g_print ("legacy response:\n%s", res);
#endif
      g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
				 res, strlen (res), NULL, NULL, NULL);
      g_free (res);
      g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
				 challenge, 16, NULL, NULL, NULL);
      proto_v7_plus = FALSE;
    }

  input = g_new0 (BroadwayInput, 1);

  input->display = request->display;
  input->connection = g_object_ref (request->connection);
  input->proto_v7_plus = proto_v7_plus;
  input->binary = binary;

  data_buffer = g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (request->data), &data_buffer_size);
  input->buffer = g_byte_array_sized_new (data_buffer_size);
  g_byte_array_append (input->buffer, data_buffer, data_buffer_size);

  broadway_display->input = input;

  start_output (request, proto_v7_plus, binary);

  /* This will free and close the data input stream, but we got all the buffered content already */
  http_request_free (request);

  in = g_io_stream_get_input_stream (G_IO_STREAM (input->connection));
  input->source = g_pollable_input_stream_create_source (G_POLLABLE_INPUT_STREAM (in), NULL);
  g_source_set_callback (input->source, (GSourceFunc)input_data_cb, input, NULL);
  g_source_attach (input->source, NULL);

  /* Process any data in the pipe already */
  parse_input (input);
  process_input_messages (broadway_display);

  g_strfreev (lines);
}

static void
start_output (HttpRequest *request, gboolean proto_v7_plus, gboolean binary)
{
  GSocket *socket;
  GdkBroadwayDisplay *broadway_display;
  int flag = 1;

  socket = g_socket_connection_get_socket (request->connection);
  setsockopt(g_socket_get_fd (socket), IPPROTO_TCP,
	     TCP_NODELAY, (char *) &flag, sizeof(int));

  broadway_display = GDK_BROADWAY_DISPLAY (request->display);

  if (broadway_display->output)
    {
      broadway_display->saved_serial = broadway_output_get_next_serial (broadway_display->output);
      broadway_output_free (broadway_display->output);
    }

  broadway_display->output =
    broadway_output_new (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
			 broadway_display->saved_serial, proto_v7_plus, binary);

  _gdk_broadway_resync_windows ();

  if (broadway_display->pointer_grab_window)
    broadway_output_grab_pointer (broadway_display->output,
				  GDK_WINDOW_IMPL_BROADWAY (broadway_display->pointer_grab_window->impl)->id,
				  broadway_display->pointer_grab_owner_events);
}

static void
send_data (HttpRequest *request,
	     const char *mimetype,
	     const char *data, gsize len)
{
  char *res;

  res = g_strdup_printf ("HTTP/1.0 200 OK\r\n"
			 "Content-Type: %s\r\n"
			 "Content-Length: %"G_GSIZE_FORMAT"\r\n"
			 "\r\n",
			 mimetype, len);
  /* TODO: This should really be async */
  g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
			     res, strlen (res), NULL, NULL, NULL);
  g_free (res);
  g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
			     data, len, NULL, NULL, NULL);
  http_request_free (request);
}

#include "clienthtml.h"
#include "broadwayjs.h"

static void
got_request (HttpRequest *request)
{
  char *start, *escaped, *tmp, *version, *query;

  if (!g_str_has_prefix (request->request->str, "GET "))
    {
      send_error (request, 501, "Only GET implemented");
      return;
    }

  start = request->request->str + 4; /* Skip "GET " */

  while (*start == ' ')
    start++;

  for (tmp = start; *tmp != 0 && *tmp != ' ' && *tmp != '\n'; tmp++)
    ;
  escaped = g_strndup (start, tmp - start);
  version = NULL;
  if (*tmp == ' ')
    {
      start = tmp;
      while (*start == ' ')
	start++;
      for (tmp = start; *tmp != 0 && *tmp != ' ' && *tmp != '\n'; tmp++)
	;
      version = g_strndup (start, tmp - start);
    }

  query = strchr (escaped, '?');
  if (query)
    *query = 0;

  if (strcmp (escaped, "/client.html") == 0 || strcmp (escaped, "/") == 0)
    send_data (request, "text/html", client_html, G_N_ELEMENTS(client_html) - 1);
  else if (strcmp (escaped, "/broadway.js") == 0)
    send_data (request, "text/javascript", broadway_js, G_N_ELEMENTS(broadway_js) - 1);
  else if (strcmp (escaped, "/socket") == 0)
    start_input (request, FALSE);
  else if (strcmp (escaped, "/socket-bin") == 0)
    start_input (request, TRUE);
  else
    send_error (request, 404, "File not found");

  g_free (escaped);
  g_free (version);
}

static void
got_http_request_line (GInputStream *stream,
		       GAsyncResult *result,
		       HttpRequest *request)
{
  char *line;

  line = g_data_input_stream_read_line_finish (G_DATA_INPUT_STREAM (stream), result, NULL, NULL);
  if (line == NULL)
    {
      http_request_free (request);
      g_printerr ("Error reading request lines\n");
      return;
    }
  if (strlen (line) == 0)
    got_request (request);
  else
    {
      /* Protect against overflow in request length */
      if (request->request->len > 1024 * 5)
	{
	  send_error (request, 400, "Request too long");
	}
      else
	{
	  g_string_append_printf (request->request, "%s\n", line);
	  g_data_input_stream_read_line_async (request->data, 0, NULL,
					       (GAsyncReadyCallback)got_http_request_line, request);
	}
    }
  g_free (line);
}

static gboolean
handle_incoming_connection (GSocketService    *service,
			    GSocketConnection *connection,
			    GObject           *source_object)
{
  HttpRequest *request;
  GInputStream *in;

  request = g_new0 (HttpRequest, 1);
  request->connection = g_object_ref (connection);
  request->display = (GdkDisplay *) source_object;
  request->request = g_string_new ("");

  in = g_io_stream_get_input_stream (G_IO_STREAM (connection));

  request->data = g_data_input_stream_new (in);
  g_filter_input_stream_set_close_base_stream (G_FILTER_INPUT_STREAM (request->data), FALSE);
  /* Be tolerant of input */
  g_data_input_stream_set_newline_type (request->data, G_DATA_STREAM_NEWLINE_TYPE_ANY);

  g_data_input_stream_read_line_async (request->data, 0, NULL,
				       (GAsyncReadyCallback)got_http_request_line, request);
  return TRUE;
}

GdkDisplay *
_gdk_broadway_display_open (const gchar *display_name)
{
  GdkDisplay *display;
  GdkBroadwayDisplay *broadway_display;
  GError *error;
  int port;

  display = g_object_new (GDK_TYPE_BROADWAY_DISPLAY, NULL);
  broadway_display = GDK_BROADWAY_DISPLAY (display);

  broadway_display->output = NULL;

  /* initialize the display's screens */
  broadway_display->screens = g_new (GdkScreen *, 1);
  broadway_display->screens[0] = _gdk_broadway_screen_new (display, 0);

  /* We need to initialize events after we have the screen
   * structures in places
   */
  _gdk_broadway_screen_events_init (broadway_display->screens[0]);

  /*set the default screen */
  broadway_display->default_screen = broadway_display->screens[0];

  display->device_manager = _gdk_broadway_device_manager_new (display);

  gdk_event_init (display);

  gdk_broadway_display_init_input (display);
  _gdk_broadway_display_init_dnd (display);

  _gdk_broadway_screen_setup (broadway_display->screens[0]);

  if (display_name == NULL)
    display_name = g_getenv ("BROADWAY_DISPLAY");

  port = 0;
  if (display_name != NULL)
    port = strtol(display_name, NULL, 10);
  if (port == 0)
    port = 8080;

  broadway_display->service = g_socket_service_new ();
  if (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (broadway_display->service),
					port,
					G_OBJECT (display),
					&error))
    {
      g_printerr ("Unable to listen to port %d: %s\n", 8080, error->message);
      g_error_free (error);
      return NULL;
    }
  g_signal_connect (broadway_display->service, "incoming", G_CALLBACK (handle_incoming_connection), NULL);

  g_signal_emit_by_name (display, "opened");
  g_signal_emit_by_name (gdk_display_manager_get (), "display-opened", display);

  return display;
}


static const gchar *
gdk_broadway_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return (gchar *) "Broadway";
}

static gint
gdk_broadway_display_get_n_screens (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  return 1;
}

static GdkScreen *
gdk_broadway_display_get_screen (GdkDisplay *display,
				 gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (screen_num == 0, NULL);

  return GDK_BROADWAY_DISPLAY (display)->screens[screen_num];
}

static GdkScreen *
gdk_broadway_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_BROADWAY_DISPLAY (display)->default_screen;
}

static void
gdk_broadway_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

static void
gdk_broadway_display_sync (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

}

static void
gdk_broadway_display_flush (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (display);

  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (broadway_display->output &&
      !broadway_output_flush (broadway_display->output))
    {
      broadway_display->saved_serial = broadway_output_get_next_serial (broadway_display->output);
      broadway_output_free (broadway_display->output);
      broadway_display->output = NULL;
    }
}

static gboolean
gdk_broadway_display_has_pending (GdkDisplay *display)
{
  return FALSE;
}

static GdkWindow *
gdk_broadway_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return NULL;
}

static void
gdk_broadway_display_dispose (GObject *object)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (object);

  _gdk_broadway_display_manager_remove_display (gdk_display_manager_get (),
						GDK_DISPLAY_OBJECT (object));

  g_list_foreach (broadway_display->input_devices, (GFunc) g_object_run_dispose, NULL);

  _gdk_screen_close (broadway_display->screens[0]);

  if (broadway_display->event_source)
    {
      g_source_destroy (broadway_display->event_source);
      g_source_unref (broadway_display->event_source);
      broadway_display->event_source = NULL;
    }

  G_OBJECT_CLASS (gdk_broadway_display_parent_class)->dispose (object);
}

static void
gdk_broadway_display_finalize (GObject *object)
{
  GdkBroadwayDisplay *broadway_display = GDK_BROADWAY_DISPLAY (object);

  /* Keymap */
  if (broadway_display->keymap)
    g_object_unref (broadway_display->keymap);

  _gdk_broadway_cursor_display_finalize (GDK_DISPLAY_OBJECT(broadway_display));

  /* input GdkDevice list */
  g_list_free_full (broadway_display->input_devices, g_object_unref);
  /* Free all GdkScreens */
  g_object_unref (broadway_display->screens[0]);
  g_free (broadway_display->screens);

  G_OBJECT_CLASS (gdk_broadway_display_parent_class)->finalize (object);
}

void
_gdk_broadway_display_make_default (GdkDisplay *display)
{
}

static void
gdk_broadway_display_notify_startup_complete (GdkDisplay  *display,
					      const gchar *startup_id)
{
}

static gboolean
gdk_broadway_display_supports_selection_notification (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_broadway_display_request_selection_notification (GdkDisplay *display,
						     GdkAtom     selection)

{
    return FALSE;
}

static gboolean
gdk_broadway_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

static void
gdk_broadway_display_store_clipboard (GdkDisplay    *display,
				      GdkWindow     *clipboard_window,
				      guint32        time_,
				      const GdkAtom *targets,
				      gint           n_targets)
{
}

static gboolean
gdk_broadway_display_supports_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_broadway_display_supports_input_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_broadway_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}

static GList *
gdk_broadway_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_BROADWAY_DISPLAY (display)->input_devices;
}

static gulong
gdk_broadway_display_get_next_serial (GdkDisplay *display)
{
  GdkBroadwayDisplay *broadway_display;
  broadway_display = GDK_BROADWAY_DISPLAY (display);
  if (broadway_display->output)
    return broadway_output_get_next_serial (broadway_display->output);
  return broadway_display->saved_serial;
}


static void
gdk_broadway_display_event_data_copy (GdkDisplay    *display,
				      const GdkEvent *src,
				      GdkEvent       *dst)
{
}

static void
gdk_broadway_display_event_data_free (GdkDisplay    *display,
				      GdkEvent *event)
{
}

static void
gdk_broadway_display_class_init (GdkBroadwayDisplayClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_broadway_display_dispose;
  object_class->finalize = gdk_broadway_display_finalize;

  display_class->window_type = GDK_TYPE_BROADWAY_WINDOW;

  display_class->get_name = gdk_broadway_display_get_name;
  display_class->get_n_screens = gdk_broadway_display_get_n_screens;
  display_class->get_screen = gdk_broadway_display_get_screen;
  display_class->get_default_screen = gdk_broadway_display_get_default_screen;
  display_class->beep = gdk_broadway_display_beep;
  display_class->sync = gdk_broadway_display_sync;
  display_class->flush = gdk_broadway_display_flush;
  display_class->has_pending = gdk_broadway_display_has_pending;
  display_class->queue_events = _gdk_broadway_display_queue_events;
  display_class->get_default_group = gdk_broadway_display_get_default_group;
  display_class->supports_selection_notification = gdk_broadway_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_broadway_display_request_selection_notification;
  display_class->supports_clipboard_persistence = gdk_broadway_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_broadway_display_store_clipboard;
  display_class->supports_shapes = gdk_broadway_display_supports_shapes;
  display_class->supports_input_shapes = gdk_broadway_display_supports_input_shapes;
  display_class->supports_composite = gdk_broadway_display_supports_composite;
  display_class->list_devices = gdk_broadway_display_list_devices;
  display_class->get_cursor_for_type = _gdk_broadway_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_broadway_display_get_cursor_for_name;
  display_class->get_cursor_for_pixbuf = _gdk_broadway_display_get_cursor_for_pixbuf;
  display_class->get_default_cursor_size = _gdk_broadway_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_broadway_display_get_maximal_cursor_size;
  display_class->supports_cursor_alpha = _gdk_broadway_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_broadway_display_supports_cursor_color;

  display_class->before_process_all_updates = _gdk_broadway_display_before_process_all_updates;
  display_class->after_process_all_updates = _gdk_broadway_display_after_process_all_updates;
  display_class->get_next_serial = gdk_broadway_display_get_next_serial;
  display_class->notify_startup_complete = gdk_broadway_display_notify_startup_complete;
  display_class->event_data_copy = gdk_broadway_display_event_data_copy;
  display_class->event_data_free = gdk_broadway_display_event_data_free;
  display_class->create_window_impl = _gdk_broadway_display_create_window_impl;
  display_class->get_keymap = _gdk_broadway_display_get_keymap;
  display_class->get_selection_owner = _gdk_broadway_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_broadway_display_set_selection_owner;
  display_class->send_selection_notify = _gdk_broadway_display_send_selection_notify;
  display_class->get_selection_property = _gdk_broadway_display_get_selection_property;
  display_class->convert_selection = _gdk_broadway_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_broadway_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_broadway_display_utf8_to_string_target;
}


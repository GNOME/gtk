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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkdisplay-broadway.h"

#include "gdkdisplay.h"
#include "gdkeventsource.h"
#include "gdkscreen.h"
#include "gdkscreen-broadway.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static void   gdk_display_broadway_dispose            (GObject            *object);
static void   gdk_display_broadway_finalize           (GObject            *object);

G_DEFINE_TYPE (GdkDisplayBroadway, _gdk_display_broadway, GDK_TYPE_DISPLAY)


static void
_gdk_display_broadway_class_init (GdkDisplayBroadwayClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gdk_display_broadway_dispose;
  object_class->finalize = gdk_display_broadway_finalize;
}

static void
_gdk_display_broadway_init (GdkDisplayBroadway *display)
{
  display->id_ht = g_hash_table_new (NULL, NULL);
}

static void
_gdk_event_init (GdkDisplay *display)
{
  GdkDisplayBroadway *display_broadway;

  display_broadway = GDK_DISPLAY_BROADWAY (display);
  display_broadway->event_source = gdk_event_source_new (display);
}

static void
_gdk_input_init (GdkDisplay *display)
{
  GdkDisplayBroadway *display_broadway;
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  GList *list, *l;

  display_broadway = GDK_DISPLAY_BROADWAY (display);
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

      display_broadway->input_devices = g_list_prepend (display_broadway->input_devices,
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
  display_broadway->input_devices = g_list_prepend (display_broadway->input_devices,
                                               g_object_ref (display->core_pointer));

  g_list_free (list);
}

typedef struct {
  GdkDisplay *display;
  GSocketConnection *connection;
  GDataInputStream *data;
  GString *request;
} HttpRequest;

static void
http_request_free (HttpRequest *request)
{
  g_object_unref (request->connection);
  g_object_unref (request->data);
  g_string_free (request->request, TRUE);
  g_free (request);
}

#include <unistd.h>
#include <fcntl.h>
static void
set_fd_blocking (int fd)
{
  glong arg;

  if ((arg = fcntl (fd, F_GETFL, NULL)) < 0)
    arg = 0;

  arg = arg & ~O_NONBLOCK;

  fcntl (fd, F_SETFL, arg);
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
got_input (GInputStream *stream,
	   GAsyncResult *result,
	   HttpRequest *request)
{
  GError *error;
  char *message;
  gsize len;

  error = NULL;
  message = g_data_input_stream_read_upto_finish (G_DATA_INPUT_STREAM (stream), result, &len, &error);
  if (message == NULL)
    {
      g_print (error->message);
      g_error_free (error);
      exit (1);
    }
  g_assert (message[0] == 0);
  _gdk_events_got_input (request->display, message + 1);

  /* Skip past ending 0xff */
  g_data_input_stream_read_byte (request->data, NULL, NULL);
  g_data_input_stream_read_upto_async (request->data, "\xff", 1, 0, NULL,
				       (GAsyncReadyCallback)got_input, request);
}

static void
start_input (HttpRequest *request)
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

  lines = g_strsplit (request->request->str, "\n", 0);

  num_key1 = 0;
  num_key2 = 0;
  key1 = 0;
  key2 = 0;
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
      else if ((p = parse_line (lines[i], "Origin")))
	{
	  origin = p;
	}
      else if ((p = parse_line (lines[i], "Host")))
	{
	  host = p;
	}
    }


  if (num_key1 != 1 || num_key2 != 1 || origin == NULL || host == NULL)
    {
      g_print ("error");
      exit (1);
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
      g_print ("error");
      exit (1);
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
			 "Sec-WebSocket-Location: ws://%s/input\r\n"
			 "Sec-WebSocket-Protocol: broadway\r\n"
			 "\r\n",
			 origin, host);

  /* TODO: This should really be async */
  g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
			     res, strlen (res), NULL, NULL, NULL);
  g_free (res);
  g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
			     challenge, 16, NULL, NULL, NULL);

  g_data_input_stream_read_upto_async (request->data, "\xff", 1, 0, NULL,
				       (GAsyncReadyCallback)got_input, request);
}

static void
start_output (HttpRequest *request)
{
  GSocket *socket;
  GdkDisplayBroadway *display_broadway;
  int fd;

  socket = g_socket_connection_get_socket (request->connection);

  display_broadway = GDK_DISPLAY_BROADWAY (request->display);
  fd = g_socket_get_fd (socket);
  set_fd_blocking (fd);
  /* We dup this because otherwise it'll be closed with the request SocketConnection */
  display_broadway->output = broadway_output_new (dup(fd));
  _gdk_broadway_resync_windows ();
  http_request_free (request);
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
  char *start, *escaped, *tmp, *version;

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

  if (strcmp (escaped, "/client.html") == 0 || strcmp (escaped, "/") == 0)
    send_data (request, "text/html", client_html, G_N_ELEMENTS(client_html) - 1);
  else if (strcmp (escaped, "/broadway.js") == 0)
    send_data (request, "text/javascript", broadway_js, G_N_ELEMENTS(broadway_js) - 1);
  else if (strcmp (escaped, "/output") == 0)
    start_output (request);
  else if (strcmp (escaped, "/input") == 0)
    start_input (request);
  else
    send_error (request, 404, "File not found");
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
	  send_error (request, 400, "Request to long");
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
gdk_display_open (const gchar *display_name)
{
  GdkDisplay *display;
  GdkDisplayBroadway *display_broadway;
  const char *sm_client_id;
  GError *error;

  display = g_object_new (GDK_TYPE_DISPLAY_BROADWAY, NULL);
  display_broadway = GDK_DISPLAY_BROADWAY (display);

  display_broadway->output = NULL;

  /* initialize the display's screens */
  display_broadway->screens = g_new (GdkScreen *, 1);
  display_broadway->screens[0] = _gdk_broadway_screen_new (display, 0);

  /* We need to initialize events after we have the screen
   * structures in places
   */
  _gdk_screen_broadway_events_init (display_broadway->screens[0]);

  /*set the default screen */
  display_broadway->default_screen = display_broadway->screens[0];

  display->device_manager = _gdk_device_manager_new (display);

  _gdk_event_init (display);

  sm_client_id = _gdk_get_sm_client_id ();
  if (sm_client_id)
    _gdk_windowing_display_set_sm_client_id (display, sm_client_id);

  _gdk_input_init (display);
  _gdk_dnd_init (display);

  _gdk_broadway_screen_setup (display_broadway->screens[0]);

  display_broadway->service = g_socket_service_new ();
  if (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (display_broadway->service),
					8080,
					G_OBJECT (display),
					&error))
    {
      g_printerr ("Unable to listen to port %d: %s\n", 8080, error->message);
      g_error_free (error);
      return NULL;
    }
  g_signal_connect (display_broadway->service, "incoming", G_CALLBACK (handle_incoming_connection), NULL);

  g_signal_emit_by_name (display, "opened");
  g_signal_emit_by_name (gdk_display_manager_get (), "display-opened", display);

  return display;
}


G_CONST_RETURN gchar *
gdk_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return (gchar *) "Broadway";
}

gint
gdk_display_get_n_screens (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  return 1;
}

GdkScreen *
gdk_display_get_screen (GdkDisplay *display,
			gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (screen_num == 0, NULL);

  return GDK_DISPLAY_BROADWAY (display)->screens[screen_num];
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_BROADWAY (display)->default_screen;
}

void
gdk_device_ungrab (GdkDevice  *device,
                   guint32     time_)
{
}

void
gdk_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

void
gdk_display_sync (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

}

void
gdk_display_flush (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

}

GdkWindow *
gdk_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return NULL;
}

void
gdk_broadway_display_grab (GdkDisplay *display)
{
}

void
gdk_broadway_display_ungrab (GdkDisplay *display)
{
}

static void
gdk_display_broadway_dispose (GObject *object)
{
  GdkDisplayBroadway *display_broadway = GDK_DISPLAY_BROADWAY (object);

  g_list_foreach (display_broadway->input_devices, (GFunc) g_object_run_dispose, NULL);

  _gdk_screen_close (display_broadway->screens[0]);

  if (display_broadway->event_source)
    {
      g_source_destroy (display_broadway->event_source);
      g_source_unref (display_broadway->event_source);
      display_broadway->event_source = NULL;
    }

  G_OBJECT_CLASS (_gdk_display_broadway_parent_class)->dispose (object);
}

static void
gdk_display_broadway_finalize (GObject *object)
{
  GdkDisplayBroadway *display_broadway = GDK_DISPLAY_BROADWAY (object);

  /* Keymap */
  if (display_broadway->keymap)
    g_object_unref (display_broadway->keymap);

  _gdk_broadway_cursor_display_finalize (GDK_DISPLAY_OBJECT(display_broadway));

  /* Atom Hashtable */
  g_hash_table_destroy (display_broadway->atom_from_virtual);
  g_hash_table_destroy (display_broadway->atom_to_virtual);

  /* input GdkDevice list */
  g_list_foreach (display_broadway->input_devices, (GFunc) g_object_unref, NULL);
  g_list_free (display_broadway->input_devices);
  /* Free all GdkScreens */
  g_object_unref (display_broadway->screens[0]);
  g_free (display_broadway->screens);

  G_OBJECT_CLASS (_gdk_display_broadway_parent_class)->finalize (object);
}

void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
}

void
gdk_notify_startup_complete (void)
{
}

void
gdk_notify_startup_complete_with_id (const gchar* startup_id)
{
}

gboolean
gdk_display_supports_selection_notification (GdkDisplay *display)
{
  return FALSE;
}

gboolean
gdk_display_request_selection_notification (GdkDisplay *display,
					    GdkAtom     selection)

{
    return FALSE;
}

gboolean
gdk_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

void
gdk_display_store_clipboard (GdkDisplay    *display,
			     GdkWindow     *clipboard_window,
			     guint32        time_,
			     const GdkAtom *targets,
			     gint           n_targets)
{
}

guint32
gdk_broadway_display_get_user_time (GdkDisplay *display)
{
  return GDK_DISPLAY_BROADWAY (display)->user_time;
}

gboolean
gdk_display_supports_shapes (GdkDisplay *display)
{
  return FALSE;
}

gboolean
gdk_display_supports_input_shapes (GdkDisplay *display)
{
  return FALSE;
}

gboolean
gdk_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}

GList *
gdk_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_BROADWAY (display)->input_devices;
}

gboolean
gdk_event_send_client_message_for_display (GdkDisplay     *display,
					   GdkEvent       *event,
					   GdkNativeWindow winid)
{
  return FALSE;
}

void
gdk_display_add_client_message_filter (GdkDisplay   *display,
				       GdkAtom       message_type,
				       GdkFilterFunc func,
				       gpointer      data)
{
}

void
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
}

void
gdk_flush (void)
{
  GSList *tmp_list = _gdk_displays;

  while (tmp_list)
    {
      gdk_display_flush (GDK_DISPLAY_OBJECT (tmp_list->data));
      tmp_list = tmp_list->next;
    }
}

gulong
_gdk_windowing_window_get_next_serial (GdkDisplay *display)
{
  return 0;
}

#include "config.h"
#include <string.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#include <glib.h>
#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <gio/gunixsocketaddress.h>
#endif

#include "broadway-server.h"

BroadwayServer *server;
GList *clients;

static guint32 client_id_count = 1;

/* Serials:
 *
 * Broadway tracks serials for all clients primarily to get the right behaviour wrt
 * grabs. Each request the client sends gets an increasing per-client serial number, starting
 * at 1. Thus, the client can now when a mouse event is seen whether the mouse event was
 * sent before or after the server saw the grab request from the client (as this affects how
 * the event is handled).
 *
 * There is only a single stream of increasing serials sent from the daemon to the web browser
 * though, called "daemon serials", so we need to map back from the daemon serials to the client
 * serials when we send an event to a client. So, each client keeps track of the mappings
 * between its serials and daemon serials for any outstanding requests.
 *
 * There is some additional complexity in that there may be multiple consecutive web browser
 * sessions, so we need to keep track of the last daemon serial used inbetween each web client
 * connection so that the daemon serials can be strictly increasing.
 */

typedef struct {
  guint32 client_serial;
  guint32 daemon_serial;
} BroadwaySerialMapping;

typedef struct  {
  guint32 id;
  GSocketConnection *connection;
  GBufferedInputStream *in;
  GSList *serial_mappings;
  GList *windows;
  guint disconnect_idle;
} BroadwayClient;

static void
client_free (BroadwayClient *client)
{
  g_assert (client->windows == NULL);
  g_assert (client->disconnect_idle == 0);
  clients = g_list_remove (clients, client);
  g_object_unref (client->connection);
  g_object_unref (client->in);
  g_slist_free_full (client->serial_mappings, g_free);
  g_free (client);
}

static void
client_disconnected (BroadwayClient *client)
{
  GList *l;

  if (client->disconnect_idle != 0)
    {
      g_source_remove (client->disconnect_idle);
      client->disconnect_idle = 0;
    }

  for (l = client->windows; l != NULL; l = l->next)
    broadway_server_destroy_window (server,
				    GPOINTER_TO_UINT (l->data));
  g_list_free (client->windows);
  client->windows = NULL;

  broadway_server_flush (server);

  client_free (client);
}

static gboolean
disconnect_idle_cb (BroadwayClient *client)
{
  client->disconnect_idle = 0;
  client_disconnected (client);
  return G_SOURCE_REMOVE;
}

static void
client_disconnect_in_idle (BroadwayClient *client)
{
  if (client->disconnect_idle == 0)
    client->disconnect_idle =
      g_idle_add_full (G_PRIORITY_DEFAULT, (GSourceFunc)disconnect_idle_cb, client, NULL);
}

static void
send_reply (BroadwayClient *client,
	    BroadwayRequest *request,
	    BroadwayReply *reply,
	    gsize size,
	    guint32 type)
{
  GOutputStream *output;

  reply->base.size = size;
  reply->base.in_reply_to = request ? request->base.serial : 0;
  reply->base.type = type;

  output = g_io_stream_get_output_stream (G_IO_STREAM (client->connection));
  if (!g_output_stream_write_all (output, reply, size, NULL, NULL, NULL))
    {
      g_printerr ("can't write to client");
      client_disconnect_in_idle (client);
    }
}

static void
add_client_serial_mapping (BroadwayClient *client,
			   guint32 client_serial,
			   guint32 daemon_serial)
{
  BroadwaySerialMapping *map;
  GSList *last;

  last = g_slist_last (client->serial_mappings);

  if (last != NULL)
    {
      map = last->data;

      /* If we have no web client, don't grow forever */
      if (map->daemon_serial == daemon_serial)
	{
	  map->client_serial = client_serial;
	  return;
	}
    }

  map = g_new0 (BroadwaySerialMapping, 1);
  map->client_serial = client_serial;
  map->daemon_serial = daemon_serial;
  client->serial_mappings = g_slist_append (client->serial_mappings, map);
}

/* Returns the latest seen client serial at the time we sent
   a daemon request to the browser with a specific daemon serial */
static guint32
get_client_serial (BroadwayClient *client, guint32 daemon_serial)
{
  BroadwaySerialMapping *map;
  GSList *l, *found;
  guint32 client_serial = 0;

  found = NULL;
  for (l = client->serial_mappings;  l != NULL; l = l->next)
    {
      map = l->data;

      if (map->daemon_serial <= daemon_serial)
	{
	  found = l;
	  client_serial = map->client_serial;
	}
      else
	break;
    }

  /* Remove mappings before the found one, they will never more be used */
  while (found != NULL &&
	 client->serial_mappings != found)
    {
      g_free (client->serial_mappings->data);
      client->serial_mappings =
	g_slist_delete_link (client->serial_mappings, client->serial_mappings);
    }

  return client_serial;
}


static void
client_handle_request (BroadwayClient *client,
		       BroadwayRequest *request)
{
  BroadwayReplyNewWindow reply_new_window;
  BroadwayReplySync reply_sync;
  BroadwayReplyQueryMouse reply_query_mouse;
  BroadwayReplyGrabPointer reply_grab_pointer;
  BroadwayReplyUngrabPointer reply_ungrab_pointer;
  cairo_surface_t *surface;
  guint32 before_serial, now_serial;

  before_serial = broadway_server_get_next_serial (server);

  switch (request->base.type)
    {
    case BROADWAY_REQUEST_NEW_WINDOW:
      reply_new_window.id =
	broadway_server_new_window (server,
				    request->new_window.x,
				    request->new_window.y,
				    request->new_window.width,
				    request->new_window.height,
				    request->new_window.is_temp);
      client->windows =
	g_list_prepend (client->windows,
			GUINT_TO_POINTER (reply_new_window.id));

      send_reply (client, request, (BroadwayReply *)&reply_new_window, sizeof (reply_new_window),
		  BROADWAY_REPLY_NEW_WINDOW);
      break;
    case BROADWAY_REQUEST_FLUSH:
      broadway_server_flush (server);
      break;
    case BROADWAY_REQUEST_SYNC:
      broadway_server_flush (server);
      send_reply (client, request, (BroadwayReply *)&reply_sync, sizeof (reply_sync),
		  BROADWAY_REPLY_SYNC);
      break;
    case BROADWAY_REQUEST_QUERY_MOUSE:
      broadway_server_query_mouse (server,
				   &reply_query_mouse.toplevel,
				   &reply_query_mouse.root_x,
				   &reply_query_mouse.root_y,
				   &reply_query_mouse.mask);
      send_reply (client, request, (BroadwayReply *)&reply_query_mouse, sizeof (reply_query_mouse),
		  BROADWAY_REPLY_QUERY_MOUSE);
      break;
    case BROADWAY_REQUEST_DESTROY_WINDOW:
      client->windows =
	g_list_remove (client->windows,
		       GUINT_TO_POINTER (request->destroy_window.id));
      broadway_server_destroy_window (server, request->destroy_window.id);
      break;
    case BROADWAY_REQUEST_SHOW_WINDOW:
      broadway_server_window_show (server, request->show_window.id);
      break;
    case BROADWAY_REQUEST_HIDE_WINDOW:
      broadway_server_window_hide (server, request->hide_window.id);
      break;
    case BROADWAY_REQUEST_SET_TRANSIENT_FOR:
      broadway_server_window_set_transient_for (server,
						request->set_transient_for.id,
						request->set_transient_for.parent);
      break;
    case BROADWAY_REQUEST_UPDATE:
      surface = broadway_server_open_surface (server,
					      request->update.id,
					      request->update.name,
					      request->update.width,
					      request->update.height);
      if (surface != NULL)
	{
	  broadway_server_window_update (server,
					 request->update.id,
					 surface);
	  cairo_surface_destroy (surface);
	}
      break;
    case BROADWAY_REQUEST_MOVE_RESIZE:
      broadway_server_window_move_resize (server,
					  request->move_resize.id,
					  request->move_resize.with_move,
					  request->move_resize.x,
					  request->move_resize.y,
					  request->move_resize.width,
					  request->move_resize.height);
      break;
    case BROADWAY_REQUEST_GRAB_POINTER:
      reply_grab_pointer.status =
	broadway_server_grab_pointer (server,
				      client->id,
				      request->grab_pointer.id,
				      request->grab_pointer.owner_events,
				      request->grab_pointer.event_mask,
				      request->grab_pointer.time_);
      send_reply (client, request, (BroadwayReply *)&reply_grab_pointer, sizeof (reply_grab_pointer),
		  BROADWAY_REPLY_GRAB_POINTER);
      break;
    case BROADWAY_REQUEST_UNGRAB_POINTER:
      reply_ungrab_pointer.status =
	broadway_server_ungrab_pointer (server,
					request->ungrab_pointer.time_);
      send_reply (client, request, (BroadwayReply *)&reply_ungrab_pointer, sizeof (reply_ungrab_pointer),
		  BROADWAY_REPLY_UNGRAB_POINTER);
      break;
    case BROADWAY_REQUEST_FOCUS_WINDOW:
      broadway_server_focus_window (server, request->focus_window.id);
      break;
    case BROADWAY_REQUEST_SET_SHOW_KEYBOARD:
      broadway_server_set_show_keyboard (server, request->set_show_keyboard.show_keyboard);
      break;
    default:
      g_warning ("Unknown request of type %d\n", request->base.type);
    }


  now_serial = broadway_server_get_next_serial (server);

  /* If we sent a new output request, map that this client serial to that, otherwise
     update old mapping for previously sent daemon serial */
  if (now_serial != before_serial)
    add_client_serial_mapping (client,
			       request->base.serial,
			       before_serial);
  else
    add_client_serial_mapping (client,
			       request->base.serial,
			       before_serial - 1);
}

static void
client_fill_cb (GObject *source_object,
		GAsyncResult *result,
		gpointer user_data)
{
  BroadwayClient *client = user_data;
  gssize res;

  res = g_buffered_input_stream_fill_finish (client->in, result, NULL);
  
  if (res > 0)
    {
      guint32 size;
      gsize count, remaining;
      guint8 *buffer;

      buffer = (guint8 *)g_buffered_input_stream_peek_buffer (client->in, &count);

      remaining = count;
      while (remaining >= sizeof (guint32))
	{
	  memcpy (&size, buffer, sizeof (guint32));

	  if (size <= remaining)
	    {
	      client_handle_request (client, (BroadwayRequest *)buffer);

	      remaining -= size;
	      buffer += size;
	    }
	}
      
      /* This is guaranteed not to block */
      g_input_stream_skip (G_INPUT_STREAM (client->in), count - remaining, NULL, NULL);
      
      g_buffered_input_stream_fill_async (client->in,
					  4*1024,
					  0,
					  NULL,
					  client_fill_cb, client);
    }
  else
    {
      client_disconnected (client);
    }
}


static gboolean
incoming_client (GSocketService    *service,
		 GSocketConnection *connection,
		 GObject           *source_object)
{
  BroadwayClient *client;
  GInputStream *input;
  BroadwayInputMsg ev = { {0} };

  client = g_new0 (BroadwayClient, 1);
  client->id = client_id_count++;
  client->connection = g_object_ref (connection);

  input = g_io_stream_get_input_stream (G_IO_STREAM (client->connection));
  client->in = (GBufferedInputStream *)g_buffered_input_stream_new (input);

  clients = g_list_prepend (clients, client);

  g_buffered_input_stream_fill_async (client->in,
				      4*1024,
				      0,
				      NULL,
				      client_fill_cb, client);

  /* Send initial resize notify */
  ev.base.type = BROADWAY_EVENT_SCREEN_SIZE_CHANGED;
  ev.base.serial = broadway_server_get_next_serial (server) - 1;
  ev.base.time = broadway_server_get_last_seen_time (server);
  broadway_server_get_screen_size (server,
				   &ev.screen_resize_notify.width,
				   &ev.screen_resize_notify.height);

  broadway_events_got_input (&ev,
			     client->id);

  return TRUE;
}


int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;
  GMainLoop *loop;
  GInetAddress *inet;
  GSocketAddress *address;
  GSocketService *listener;
  char *path, *basename;
  char *http_address = NULL;
  char *unixsocket_address = NULL;
  int http_port = 0;
  char *ssl_cert = NULL;
  char *ssl_key = NULL;
  char *display;
  int port = 0;
  const GOptionEntry entries[] = {
    { "port", 'p', 0, G_OPTION_ARG_INT, &http_port, "Httpd port", "PORT" },
    { "address", 'a', 0, G_OPTION_ARG_STRING, &http_address, "Ip address to bind to ", "ADDRESS" },
#ifdef G_OS_UNIX
    { "unixsocket", 'u', 0, G_OPTION_ARG_STRING, &unixsocket_address, "Unix domain socket address", "ADDRESS" },
#endif
    { "cert", 'c', 0, G_OPTION_ARG_STRING, &ssl_cert, "SSL certificate path", "PATH" },
    { "key", 'k', 0, G_OPTION_ARG_STRING, &ssl_key, "SSL key path", "PATH" },
    { NULL }
  };

  context = g_option_context_new ("[:DISPLAY] - broadway display daemon");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("option parsing failed: %s\n", error->message);
      exit (1);
    }

  display = NULL;
  if (argc > 1)
    {
      if (*argv[1] != ':')
	{
	  g_printerr ("Usage broadwayd [:DISPLAY]\n");
	  exit (1);
	}
      display = argv[1];
    }

  if (display == NULL)
    {
#ifdef G_OS_UNIX
      display = ":0";
#else
      display = ":tcp";
#endif
    }

  if (g_str_has_prefix (display, ":tcp"))
    {
      port = strtol (display + strlen (":tcp"), NULL, 10);

      inet = g_inet_address_new_from_string ("127.0.0.1");
      g_print ("Listening on 127.0.0.1:%d\n", port + 9090);
      address = g_inet_socket_address_new (inet, port + 9090);
      g_object_unref (inet);
    }
#ifdef G_OS_UNIX
  else if (display[0] == ':' && g_ascii_isdigit(display[1]))
    {
      port = strtol (display + strlen (":"), NULL, 10);
      basename = g_strdup_printf ("broadway%d.socket", port + 1);
      path = g_build_filename (g_get_user_runtime_dir (), basename, NULL);
      g_free (basename);

      g_print ("Listening on %s\n", path);
      address = g_unix_socket_address_new_with_type (path, -1,
						     G_UNIX_SOCKET_ADDRESS_ABSTRACT);
      g_free (path);
    }
#endif
  else
    {
      g_printerr ("Failed to parse display %s\n", display);
      exit (1);
    }

  if (http_port == 0)
    http_port = 8080 + port;

  if (unixsocket_address != NULL)
    server = broadway_server_on_unix_socket_new (unixsocket_address, &error);
  else
    server = broadway_server_new (http_address,
                                  http_port,
                                  ssl_cert,
                                  ssl_key,
                                  &error);

  if (server == NULL)
    {
      g_printerr ("%s\n", error->message);
      return 1;
    }

  listener = g_socket_service_new ();
  if (!g_socket_listener_add_address (G_SOCKET_LISTENER (listener),
				      address,
				      G_SOCKET_TYPE_STREAM,
				      G_SOCKET_PROTOCOL_DEFAULT,
				      G_OBJECT (server),
				      NULL,
				      &error))
    {
      g_printerr ("Can't listen: %s\n", error->message);
      return 1;
    }
  g_object_unref (address);
  g_signal_connect (listener, "incoming", G_CALLBACK (incoming_client), NULL);

  g_socket_service_start (G_SOCKET_SERVICE (listener));

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  
  return 0;
}

static gsize
get_event_size (int type)
{
  switch (type)
    {
    case BROADWAY_EVENT_ENTER:
    case BROADWAY_EVENT_LEAVE:
      return sizeof (BroadwayInputCrossingMsg);
    case BROADWAY_EVENT_POINTER_MOVE:
      return sizeof (BroadwayInputPointerMsg);
    case BROADWAY_EVENT_BUTTON_PRESS:
    case BROADWAY_EVENT_BUTTON_RELEASE:
      return sizeof (BroadwayInputButtonMsg);
    case BROADWAY_EVENT_SCROLL:
      return sizeof (BroadwayInputScrollMsg);
    case BROADWAY_EVENT_TOUCH:
      return sizeof (BroadwayInputTouchMsg);
    case BROADWAY_EVENT_KEY_PRESS:
    case BROADWAY_EVENT_KEY_RELEASE:
      return  sizeof (BroadwayInputKeyMsg);
    case BROADWAY_EVENT_GRAB_NOTIFY:
    case BROADWAY_EVENT_UNGRAB_NOTIFY:
      return sizeof (BroadwayInputGrabReply);
    case BROADWAY_EVENT_CONFIGURE_NOTIFY:
      return  sizeof (BroadwayInputConfigureNotify);
    case BROADWAY_EVENT_DELETE_NOTIFY:
      return sizeof (BroadwayInputDeleteNotify);
    case BROADWAY_EVENT_SCREEN_SIZE_CHANGED:
      return sizeof (BroadwayInputScreenResizeNotify);
    case BROADWAY_EVENT_FOCUS:
      return sizeof (BroadwayInputFocusMsg);
    default:
      g_assert_not_reached ();
    }
  return 0;
}

void
broadway_events_got_input (BroadwayInputMsg *message,
			   gint32 client_id)
{
  GList *l;
  BroadwayReplyEvent reply_event;
  gsize size;
  guint32 daemon_serial;

  size = get_event_size (message->base.type);
  g_assert (sizeof (BroadwayReplyBase) + size <= sizeof (BroadwayReplyEvent));

  memset (&reply_event, 0, sizeof (BroadwayReplyEvent));
  daemon_serial = message->base.serial;

  memcpy (&reply_event.msg, message, size);

  for (l = clients; l != NULL; l = l->next)
    {
      BroadwayClient *client = l->data;

      if (client_id == -1 ||
	  client->id == client_id)
	{
	  reply_event.msg.base.serial = get_client_serial (client, daemon_serial);

	  send_reply (client, NULL, (BroadwayReply *)&reply_event,
		      G_STRUCT_OFFSET (BroadwayReplyEvent, msg) + size,
		      BROADWAY_REPLY_EVENT);
	}
    }
}

#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>

#include "gdkbroadway-server.h"

GdkBroadwayServer *server;
GList *clients;

static guint32 client_id_count = 1;

typedef struct  {
  guint32 id;
  GSocketConnection *connection;
  GBufferedInputStream *in;
  guint32 last_seen_serial;
  GList *windows;
  guint disconnect_idle;
} BroadwayClient;

static void
client_free (BroadwayClient *client)
{
  g_assert (client->disconnect_idle == 0);
  clients = g_list_remove (clients, client);
  g_object_unref (client->connection);
  g_object_unref (client->in);
  g_free (client);
}

static void
client_disconnected (BroadwayClient *client)
{
  if (client->disconnect_idle != 0)
    {
      g_source_remove (client->disconnect_idle);
      client->disconnect_idle = 0;
    }

  g_print ("client %d disconnected\n", client->id);

  /* TODO: destroy client windows, also maybe do this in an idle, at least in some cases like on an i/o error */

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
  reply->base.last_serial = client->last_seen_serial;
  reply->base.in_reply_to = request ? request->base.serial : 0;
  reply->base.type = type;

  output = g_io_stream_get_output_stream (G_IO_STREAM (client->connection));
  if (!g_output_stream_write_all (output, reply, size, NULL, NULL, NULL))
    {
      g_printerr ("can't write to client");
      client_disconnect_in_idle (client);
    }
}

static cairo_region_t *
region_from_rects (BroadwayRect *rects, int n_rects)
{
  cairo_region_t *region;
  cairo_rectangle_int_t *cairo_rects;
  int i;
  
  cairo_rects = g_new (cairo_rectangle_int_t, n_rects);
  for (i = 0; i < n_rects; i++)
    {
      cairo_rects[i].x = rects[i].x;
      cairo_rects[i].y = rects[i].y;
      cairo_rects[i].width = rects[i].width;
      cairo_rects[i].height = rects[i].height;
    }
  region = cairo_region_create_rectangles (cairo_rects, n_rects);
  g_free (cairo_rects);
  return region;
}

static const cairo_user_data_key_t shm_cairo_key;

typedef struct {
  void *data;
  gsize data_size;
} ShmSurfaceData;

static void
shm_data_unmap (void *_data)
{
  ShmSurfaceData *data = _data;
  munmap (data->data, data->data_size);
  g_free (data);
}

cairo_surface_t *
open_surface (char *name, int width, int height)
{
  ShmSurfaceData *data;
  cairo_surface_t *surface;
  gsize size;
  void *ptr;
  int fd;

  /* TODO: Cache this */
  
  size = width * height * sizeof (guint32);

  fd = shm_open(name, O_RDONLY, 0600);
  if (fd == -1)
    {
      perror ("Failed to shm_open");
      return NULL;
    }

  ptr = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0); 
  (void) close(fd);

  if (ptr == NULL)
    return NULL;

  data = g_new0 (ShmSurfaceData, 1);

  data->data = ptr;
  data->data_size = size;

  surface = cairo_image_surface_create_for_data ((guchar *)data->data,
						 CAIRO_FORMAT_RGB24,
						 width, height,
						 width * sizeof (guint32));
  g_assert (surface != NULL);
  
  cairo_surface_set_user_data (surface, &shm_cairo_key,
			       data, shm_data_unmap);

  return surface;
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
  cairo_region_t *area;
  cairo_surface_t *surface;

  client->last_seen_serial = request->base.serial;

  switch (request->base.type)
    {
    case BROADWAY_REQUEST_NEW_WINDOW:
      reply_new_window.id =
	_gdk_broadway_server_new_window (server,
					 request->new_window.x,
					 request->new_window.y,
					 request->new_window.width,
					 request->new_window.height,
					 request->new_window.is_temp);
      
      send_reply (client, request, (BroadwayReply *)&reply_new_window, sizeof (reply_new_window),
		  BROADWAY_REPLY_NEW_WINDOW);
      break;
    case BROADWAY_REQUEST_FLUSH:
      _gdk_broadway_server_flush (server);
      break;
    case BROADWAY_REQUEST_SYNC:
      _gdk_broadway_server_flush (server);
      send_reply (client, request, (BroadwayReply *)&reply_sync, sizeof (reply_sync),
		  BROADWAY_REPLY_SYNC);
      break;
    case BROADWAY_REQUEST_QUERY_MOUSE:
      _gdk_broadway_server_query_mouse (server,
					&reply_query_mouse.toplevel,
					&reply_query_mouse.root_x,
					&reply_query_mouse.root_y,
					&reply_query_mouse.mask);
      send_reply (client, request, (BroadwayReply *)&reply_query_mouse, sizeof (reply_query_mouse),
		  BROADWAY_REPLY_QUERY_MOUSE);
      break;
    case BROADWAY_REQUEST_DESTROY_WINDOW:
      _gdk_broadway_server_destroy_window (server, request->destroy_window.id);
      break;
    case BROADWAY_REQUEST_SHOW_WINDOW:
      _gdk_broadway_server_window_show (server, request->show_window.id);
      break;
    case BROADWAY_REQUEST_HIDE_WINDOW:
      _gdk_broadway_server_window_hide (server, request->hide_window.id);
      break;
    case BROADWAY_REQUEST_SET_TRANSIENT_FOR:
      _gdk_broadway_server_window_set_transient_for (server,
						     request->set_transient_for.id,
						     request->set_transient_for.parent);
      break;
    case BROADWAY_REQUEST_TRANSLATE:
      area = region_from_rects (request->translate.rects,
				request->translate.n_rects);
      _gdk_broadway_server_window_translate (server,
					     request->translate.id,
					     area,
					     request->translate.dx,
					     request->translate.dy);
      cairo_region_destroy (area);
      break;
    case BROADWAY_REQUEST_UPDATE:
      surface = open_surface (request->update.name,
			      request->update.width,
			      request->update.height);
      if (surface != NULL)
	{
	  _gdk_broadway_server_window_update (server,
					      request->update.id,
					      surface);
	  cairo_surface_destroy (surface);
	}
      break;
    case BROADWAY_REQUEST_MOVE_RESIZE:
      if (!_gdk_broadway_server_window_move_resize (server,
						    request->move_resize.id,
						    request->move_resize.x,
						    request->move_resize.y,
						    request->move_resize.width,
						    request->move_resize.height))
	{
	  /* TODO: Send configure request */
	}
      break;
    case BROADWAY_REQUEST_GRAB_POINTER:
      reply_grab_pointer.status =
	_gdk_broadway_server_grab_pointer (server,
					   request->grab_pointer.id,
					   request->grab_pointer.owner_events,
					   request->grab_pointer.event_mask,
					   request->grab_pointer.time_);
      send_reply (client, request, (BroadwayReply *)&reply_grab_pointer, sizeof (reply_grab_pointer),
		  BROADWAY_REPLY_GRAB_POINTER);
      break;
    case BROADWAY_REQUEST_UNGRAB_POINTER:
      reply_ungrab_pointer.status =
	_gdk_broadway_server_ungrab_pointer (server,
					     request->ungrab_pointer.time_);
      send_reply (client, request, (BroadwayReply *)&reply_ungrab_pointer, sizeof (reply_ungrab_pointer),
		  BROADWAY_REPLY_UNGRAB_POINTER);
      break;
    default:
      g_warning ("Unknown request of type %d\n", request->base.type);
    }
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
      BroadwayRequest request;

      buffer = (guint8 *)g_buffered_input_stream_peek_buffer (client->in, &count);

      remaining = count;
      while (remaining >= sizeof (guint32))
	{
	  memcpy (&size, buffer, sizeof (guint32));
	  
	  if (size <= remaining)
	    {
	      g_assert (size >= sizeof (BroadwayRequestBase));
	      g_assert (size <= sizeof (BroadwayRequest));

	      memcpy (&request, buffer, size);
	      client_handle_request (client, &request);

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
  
  return TRUE;
}

int
main (int argc, char *argv[])
{
  GError *error;
  GMainLoop *loop;
  GSocketAddress *address;
  GSocketService *listener;
  char *path;

  error = NULL;
  server = _gdk_broadway_server_new (8080, &error);
  if (server == NULL)
    {
      g_printerr ("%s\n", error->message);
      return 1;
    }

  path = g_build_filename (g_get_user_runtime_dir (), "broadway1.socket", NULL);
  g_print ("Listening on %s\n", path);
  address = g_unix_socket_address_new_with_type (path, -1,
						 G_UNIX_SOCKET_ADDRESS_ABSTRACT);
  g_free (path);

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
    default:
      g_assert_not_reached ();
    }
  return 0;
}

void
_gdk_broadway_events_got_input (BroadwayInputMsg *message)
{
  GList *l;
  BroadwayReplyEvent reply_event;
  gsize size;

  size = get_event_size (message->base.type);

  reply_event.msg = *message;

  /* TODO:
     Don't send to all clients
     Rewrite serials, etc
  */
  for (l = clients; l != NULL; l = l->next)
    {
      BroadwayClient *client = l->data;

      send_reply (client, NULL, (BroadwayReply *)&reply_event,
		  sizeof (BroadwayReplyBase) + size,
		  BROADWAY_REPLY_EVENT);
    }
}

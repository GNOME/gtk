#include "config.h"

#include "broadway-server.h"

#include "broadway-output.h"

#include <glib.h>
#include <glib/gprintf.h>
#include "gdktypes.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#elif defined (G_OS_WIN32)
#include <io.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef G_OS_UNIX
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif
#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif
#ifdef G_OS_WIN32
#include <windows.h>
#include <string.h>
#endif

typedef struct BroadwayInput BroadwayInput;
typedef struct BroadwayWindow BroadwayWindow;
struct _BroadwayServer {
  GObject parent_instance;

  char *address;
  int port;
  GSocketService *service;
  BroadwayOutput *output;
  guint32 id_counter;
  guint32 saved_serial;
  guint64 last_seen_time;
  BroadwayInput *input;
  GList *input_messages;
  guint process_input_idle;

  GHashTable *id_ht;
  GList *toplevels;
  BroadwayWindow *root;
  gint32 focused_window_id; /* -1 => none */
  gint show_keyboard;

  guint32 screen_width;
  guint32 screen_height;

  gint32 mouse_in_toplevel_id;
  int last_x, last_y; /* in root coords */
  guint32 last_state;
  gint32 real_mouse_in_toplevel_id; /* Not affected by grabs */

  /* Explicit pointer grabs: */
  gint32 pointer_grab_window_id; /* -1 => none */
  gint32 pointer_grab_client_id; /* -1 => none */
  guint32 pointer_grab_time;
  gboolean pointer_grab_owner_events;

  /* Future data, from the currently queued events */
  int future_root_x;
  int future_root_y;
  guint32 future_state;
  int future_mouse_in_toplevel;
};

struct _BroadwayServerClass
{
  GObjectClass parent_class;
};

typedef struct HttpRequest {
  BroadwayServer *server;
  GSocketConnection *connection;
  GDataInputStream *data;
  GString *request;
}  HttpRequest;

struct BroadwayInput {
  BroadwayServer *server;
  BroadwayOutput *output;
  GSocketConnection *connection;
  GByteArray *buffer;
  GSource *source;
  gboolean seen_time;
  gint64 time_base;
  gboolean active;
};

struct BroadwayWindow {
  gint32 id;
  gint32 x;
  gint32 y;
  gint32 width;
  gint32 height;
  gboolean is_temp;
  gboolean visible;
  gint32 transient_for;

  BroadwayBuffer *buffer;
  gboolean buffer_synced;

  char *cached_surface_name;
  cairo_surface_t *cached_surface;
};

static void broadway_server_resync_windows (BroadwayServer *server);

G_DEFINE_TYPE (BroadwayServer, broadway_server, G_TYPE_OBJECT)

static void
broadway_server_init (BroadwayServer *server)
{
  BroadwayWindow *root;

  server->service = g_socket_service_new ();
  server->pointer_grab_window_id = -1;
  server->saved_serial = 1;
  server->last_seen_time = 1;
  server->id_ht = g_hash_table_new (NULL, NULL);
  server->id_counter = 0;

  root = g_new0 (BroadwayWindow, 1);
  root->id = server->id_counter++;
  root->width = 1024;
  root->height = 768;
  root->visible = TRUE;

  server->root = root;

  g_hash_table_insert (server->id_ht,
		       GINT_TO_POINTER (root->id),
		       root);
}

static void
broadway_server_finalize (GObject *object)
{
  BroadwayServer *server = BROADWAY_SERVER (object);

  g_free (server->address);

  G_OBJECT_CLASS (broadway_server_parent_class)->finalize (object);
}

static void
broadway_server_class_init (BroadwayServerClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = broadway_server_finalize;
}

static void start (BroadwayInput *input);

static void
http_request_free (HttpRequest *request)
{
  g_object_unref (request->connection);
  g_object_unref (request->data);
  g_string_free (request->request, TRUE);
  g_free (request);
}

static void
broadway_input_free (BroadwayInput *input)
{
  g_object_unref (input->connection);
  g_byte_array_free (input->buffer, FALSE);
  g_source_destroy (input->source);
  g_free (input);
}

static void
update_event_state (BroadwayServer *server,
		    BroadwayInputMsg *message)
{
  BroadwayWindow *window;

  switch (message->base.type) {
  case BROADWAY_EVENT_ENTER:
    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_toplevel_id = message->pointer.mouse_window_id;

    /* TODO: Unset when it dies */
    server->mouse_in_toplevel_id = message->pointer.event_window_id;
    break;
  case BROADWAY_EVENT_LEAVE:
    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_toplevel_id = message->pointer.mouse_window_id;

    server->mouse_in_toplevel_id = 0;
    break;
  case BROADWAY_EVENT_POINTER_MOVE:
    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_toplevel_id = message->pointer.mouse_window_id;
    break;
  case BROADWAY_EVENT_BUTTON_PRESS:
  case BROADWAY_EVENT_BUTTON_RELEASE:
    if (message->base.type == BROADWAY_EVENT_BUTTON_PRESS &&
        server->focused_window_id != message->pointer.mouse_window_id &&
        server->pointer_grab_window_id == -1)
      {
        broadway_server_window_raise (server, message->pointer.mouse_window_id);
        broadway_server_focus_window (server, message->pointer.mouse_window_id);
        broadway_server_flush (server);
      }

    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_toplevel_id = message->pointer.mouse_window_id;
    break;
  case BROADWAY_EVENT_SCROLL:
    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_toplevel_id = message->pointer.mouse_window_id;
    break;
  case BROADWAY_EVENT_TOUCH:
    if (message->touch.touch_type == 0 && message->touch.is_emulated &&
        server->focused_window_id != message->touch.event_window_id)
      {
        broadway_server_window_raise (server, message->touch.event_window_id);
        broadway_server_focus_window (server, message->touch.event_window_id);
        broadway_server_flush (server);
      }

    if (message->touch.is_emulated)
      {
        server->last_x = message->pointer.root_x;
        server->last_y = message->pointer.root_y;
      }

    server->last_state = message->touch.state;
    break;
  case BROADWAY_EVENT_KEY_PRESS:
  case BROADWAY_EVENT_KEY_RELEASE:
    server->last_state = message->key.state;
    break;
  case BROADWAY_EVENT_GRAB_NOTIFY:
  case BROADWAY_EVENT_UNGRAB_NOTIFY:
    break;
  case BROADWAY_EVENT_CONFIGURE_NOTIFY:
    window = g_hash_table_lookup (server->id_ht,
				  GINT_TO_POINTER (message->configure_notify.id));
    if (window != NULL)
      {
	window->x = message->configure_notify.x;
	window->y = message->configure_notify.y;
      }
    break;
  case BROADWAY_EVENT_DELETE_NOTIFY:
    break;
  case BROADWAY_EVENT_SCREEN_SIZE_CHANGED:
    server->root->width = message->screen_resize_notify.width;
    server->root->height = message->screen_resize_notify.height;
    break;

  default:
    g_printerr ("update_event_state - Unknown input command %c\n", message->base.type);
    break;
  }
}

gboolean
broadway_server_lookahead_event (BroadwayServer  *server,
				 const char         *types)
{
  BroadwayInputMsg *message;
  GList *l;

  for (l = server->input_messages; l != NULL; l = l->next)
    {
      message = l->data;
      if (strchr (types, message->base.type) != NULL)
	return TRUE;
    }

  return FALSE;
}

static gboolean
is_pointer_event (BroadwayInputMsg *message)
{
  return
    message->base.type == BROADWAY_EVENT_ENTER ||
    message->base.type == BROADWAY_EVENT_LEAVE ||
    message->base.type == BROADWAY_EVENT_POINTER_MOVE ||
    message->base.type == BROADWAY_EVENT_BUTTON_PRESS ||
    message->base.type == BROADWAY_EVENT_BUTTON_RELEASE ||
    message->base.type == BROADWAY_EVENT_SCROLL ||
    message->base.type == BROADWAY_EVENT_GRAB_NOTIFY ||
    message->base.type == BROADWAY_EVENT_UNGRAB_NOTIFY;
}

static void
process_input_message (BroadwayServer *server,
		       BroadwayInputMsg *message)
{
  gint32 client;

  update_event_state (server, message);
  client = -1;
  if (is_pointer_event (message) &&
      server->pointer_grab_window_id != -1)
    client = server->pointer_grab_client_id;

  broadway_events_got_input (message, client);
}

static void
process_input_messages (BroadwayServer *server)
{
  BroadwayInputMsg *message;

  while (server->input_messages)
    {
      message = server->input_messages->data;
      server->input_messages =
	g_list_delete_link (server->input_messages,
			    server->input_messages);

      if (message->base.serial == 0)
	{
	  /* This was sent before we got any requests, but we don't want the
	     daemon serials to go backwards, so we fix it up to be the last used
	     serial */
	  message->base.serial = server->saved_serial - 1;
	}

      process_input_message (server, message);
      g_free (message);
    }
}

static void
fake_configure_notify (BroadwayServer *server,
		       BroadwayWindow *window)
{
  BroadwayInputMsg ev = { {0} };

  ev.base.type = BROADWAY_EVENT_CONFIGURE_NOTIFY;
  ev.base.serial = server->saved_serial - 1;
  ev.base.time = server->last_seen_time;
  ev.configure_notify.id = window->id;
  ev.configure_notify.x = window->x;
  ev.configure_notify.y = window->y;
  ev.configure_notify.width = window->width;
  ev.configure_notify.height = window->height;

  process_input_message (server, &ev);
}

static guint32 *
parse_pointer_data (guint32 *p, BroadwayInputPointerMsg *data)
{
  data->mouse_window_id = ntohl (*p++);
  data->event_window_id = ntohl (*p++);
  data->root_x = ntohl (*p++);
  data->root_y = ntohl (*p++);
  data->win_x = ntohl (*p++);
  data->win_y = ntohl (*p++);
  data->state = ntohl (*p++);

  return p;
}

static guint32 *
parse_touch_data (guint32 *p, BroadwayInputTouchMsg *data)
{
  data->touch_type = ntohl (*p++);
  data->event_window_id = ntohl (*p++);
  data->sequence_id = ntohl (*p++);
  data->is_emulated = ntohl (*p++);
  data->root_x = ntohl (*p++);
  data->root_y = ntohl (*p++);
  data->win_x = ntohl (*p++);
  data->win_y = ntohl (*p++);
  data->state = ntohl (*p++);

  return p;
}

static void
update_future_pointer_info (BroadwayServer *server, BroadwayInputPointerMsg *data)
{
  server->future_root_x = data->root_x;
  server->future_root_y = data->root_y;
  server->future_state = data->state;
  server->future_mouse_in_toplevel = data->mouse_window_id;
}

static void
parse_input_message (BroadwayInput *input, const unsigned char *message)
{
  BroadwayServer *server = input->server;
  BroadwayInputMsg msg;
  guint32 *p;
  gint64 time_;

  memset (&msg, 0, sizeof (msg));

  p = (guint32 *) message;

  msg.base.type = ntohl (*p++);
  msg.base.serial = ntohl (*p++);
  time_ = ntohl (*p++);

  if (time_ == 0) {
    time_ = server->last_seen_time;
  } else {
    if (!input->seen_time) {
      input->seen_time = TRUE;
      /* Calculate time base so that any following times are normalized to start
	 5 seconds after last_seen_time, to avoid issues that could appear when
	 a long hiatus due to a reconnect seems to be instant */
      input->time_base = time_ - (server->last_seen_time + 5000);
    }
    time_ = time_ - input->time_base;
  }

  server->last_seen_time = time_;

  msg.base.time = time_;

  switch (msg.base.type) {
  case BROADWAY_EVENT_ENTER:
  case BROADWAY_EVENT_LEAVE:
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (server, &msg.pointer);
    msg.crossing.mode = ntohl (*p++);
    break;

  case BROADWAY_EVENT_POINTER_MOVE: /* Mouse move */
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (server, &msg.pointer);
    break;

  case BROADWAY_EVENT_BUTTON_PRESS:
  case BROADWAY_EVENT_BUTTON_RELEASE:
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (server, &msg.pointer);
    msg.button.button = ntohl (*p++);
    break;

  case BROADWAY_EVENT_SCROLL:
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (server, &msg.pointer);
    msg.scroll.dir = ntohl (*p++);
    break;

  case BROADWAY_EVENT_TOUCH:
    p = parse_touch_data (p, &msg.touch);
    break;

  case BROADWAY_EVENT_KEY_PRESS:
  case BROADWAY_EVENT_KEY_RELEASE:
    msg.key.window_id = server->focused_window_id;
    msg.key.key = ntohl (*p++);
    msg.key.state = ntohl (*p++);
    break;

  case BROADWAY_EVENT_GRAB_NOTIFY:
  case BROADWAY_EVENT_UNGRAB_NOTIFY:
    msg.grab_reply.res = ntohl (*p++);
    break;

  case BROADWAY_EVENT_CONFIGURE_NOTIFY:
    msg.configure_notify.id = ntohl (*p++);
    msg.configure_notify.x = ntohl (*p++);
    msg.configure_notify.y = ntohl (*p++);
    msg.configure_notify.width = ntohl (*p++);
    msg.configure_notify.height = ntohl (*p++);
    break;

  case BROADWAY_EVENT_DELETE_NOTIFY:
    msg.delete_notify.id = ntohl (*p++);
    break;

  case BROADWAY_EVENT_SCREEN_SIZE_CHANGED:
    msg.screen_resize_notify.width = ntohl (*p++);
    msg.screen_resize_notify.height = ntohl (*p++);
    break;

  default:
    g_printerr ("parse_input_message - Unknown input command %c (%s)\n", msg.base.type, message);
    break;
  }

  server->input_messages = g_list_append (server->input_messages, g_memdup (&msg, sizeof (msg)));

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
  if (!input->buffer->len)
    return;

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
      case BROADWAY_WS_BINARY:
        if (!fin)
          {
#ifdef DEBUG_WEBSOCKETS
            g_warning ("can't yet accept fragmented input");
#endif
          }
        else
          {
            parse_input_message (input, data);
          }
        break;
      case BROADWAY_WS_CNX_PING:
        broadway_output_pong (input->output);
        break;
      case BROADWAY_WS_CNX_PONG:
        break; /* we never send pings, but tolerate pongs */
      case BROADWAY_WS_TEXT:
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


static gboolean
process_input_idle_cb (BroadwayServer *server)
{
  server->process_input_idle = 0;
  process_input_messages (server);
  return G_SOURCE_REMOVE;
}

static void
queue_process_input_at_idle (BroadwayServer *server)
{
  if (server->process_input_idle == 0)
    server->process_input_idle =
      g_idle_add_full (G_PRIORITY_DEFAULT, (GSourceFunc)process_input_idle_cb, server, NULL);
}

static void
broadway_server_read_all_input_nonblocking (BroadwayInput *input)
{
  GInputStream *in;
  gssize res;
  guint8 buffer[1024];
  GError *error;

  if (input == NULL)
    return;

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

      if (input->server->input == input)
	input->server->input = NULL;
      broadway_input_free (input);
      if (res < 0)
	{
	  g_printerr ("input error %s\n", error->message);
	  g_error_free (error);
	}
      return;
    }

  g_byte_array_append (input->buffer, buffer, res);

  parse_input (input);
}

static void
broadway_server_consume_all_input (BroadwayServer *server)
{
  broadway_server_read_all_input_nonblocking (server->input);

  /* Since we're parsing input but not processing the resulting messages
     we might not get a readable callback on the stream, so queue an idle to
     process the messages */
  queue_process_input_at_idle (server);
}


static gboolean
input_data_cb (GObject  *stream,
	       BroadwayInput *input)
{
  BroadwayServer *server = input->server;

  broadway_server_read_all_input_nonblocking (input);

  if (input->active)
    process_input_messages (server);

  return TRUE;
}

guint32
broadway_server_get_next_serial (BroadwayServer *server)
{
  if (server->output)
    return broadway_output_get_next_serial (server->output);

  return server->saved_serial;
}

void
broadway_server_get_screen_size (BroadwayServer   *server,
				 guint32          *width,
				 guint32          *height)
{
  *width = server->root->width;
  *height = server->root->height;
}


void
broadway_server_flush (BroadwayServer *server)
{
  if (server->output &&
      !broadway_output_flush (server->output))
    {
      server->saved_serial = broadway_output_get_next_serial (server->output);
      broadway_output_free (server->output);
      server->output = NULL;
    }
}

void
broadway_server_sync (BroadwayServer *server)
{
  broadway_server_flush (server);
}


/* TODO: This is not used atm, is it needed? */
/* Note: This may be called while handling a message (i.e. sorta recursively) */
BroadwayInputMsg *
broadway_server_block_for_input (BroadwayServer *server, char op,
				 guint32 serial, gboolean remove_message)
{
  BroadwayInputMsg *message;
  gssize res;
  guint8 buffer[1024];
  BroadwayInput *input;
  GInputStream *in;
  GList *l;

  broadway_server_flush (server);

  if (server->input == NULL)
    return NULL;

  input = server->input;

  while (TRUE) {
    /* Check for existing reply in queue */

    for (l = server->input_messages; l != NULL; l = l->next)
      {
	message = l->data;

	if (message->base.type == op)
	  {
	    if (message->base.serial == serial)
	      {
		if (remove_message)
		  server->input_messages =
		    g_list_delete_link (server->input_messages, l);
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
    queue_process_input_at_idle (server);
  }
}

static void *
map_named_shm (char *name, gsize size)
{
#ifdef G_OS_UNIX

  int fd;
  void *ptr;

  fd = shm_open(name, O_RDONLY, 0600);
  if (fd == -1)
    {
      perror ("Failed to shm_open");
      return NULL;
    }

  ptr = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);

  (void) close(fd);

  shm_unlink (name);

  return ptr;

#elif defined(G_OS_WIN32)

  int fd;
  void *ptr;
  char *shmpath;
  void *map = ((void *)-1);

  if (*name == '/')
    ++name;
  shmpath = g_build_filename (g_get_tmp_dir (), name, NULL);

  fd = open(shmpath, O_RDONLY, 0600);
  if (fd == -1)
    {
      g_free (shmpath);
      perror ("Failed to shm_open");
      return NULL;
    }

  if (size == 0)
    ptr = map;
  else
    {
      HANDLE h, fm;
      h = (HANDLE)_get_osfhandle (fd);
      fm = CreateFileMapping (h, NULL, PAGE_READONLY, 0, (DWORD)size, NULL);
      ptr = MapViewOfFile (fm, FILE_MAP_READ, 0, 0, (size_t)size);
      CloseHandle (fm);
    }

  (void) close(fd);

  remove (shmpath);
  g_free (shmpath);

  return ptr;

#else
#error "No shm mapping supported"

  return NULL;
#endif
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
  guchar digest[20];
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
start_input (HttpRequest *request)
{
  char **lines;
  char *p;
  int i;
  char *res;
  char *origin, *host;
  BroadwayInput *input;
  const void *data_buffer;
  gsize data_buffer_size;
  GInputStream *in;
  char *key;
  GSocket *socket;
  int flag = 1;

#ifdef DEBUG_WEBSOCKETS
  g_print ("incoming request:\n%s\n", request->request->str);
#endif
  lines = g_strsplit (request->request->str, "\n", 0);

  key = NULL;
  origin = NULL;
  host = NULL;
  for (i = 0; lines[i] != NULL; i++)
    {
      if ((p = parse_line (lines[i], "Sec-WebSocket-Key")))
        key = p;
      else if ((p = parse_line (lines[i], "Origin")))
        origin = p;
      else if ((p = parse_line (lines[i], "Host")))
        host = p;
      else if ((p = parse_line (lines[i], "Sec-WebSocket-Origin")))
        origin = p;
    }

  if (host == NULL)
    {
      g_strfreev (lines);
      send_error (request, 400, "Bad websocket request");
      return;
    }

  if (key != NULL)
    {
      char* accept = generate_handshake_response_wsietf_v7 (key);
      res = g_strdup_printf ("HTTP/1.1 101 Switching Protocols\r\n"
			     "Upgrade: websocket\r\n"
			     "Connection: Upgrade\r\n"
			     "Sec-WebSocket-Accept: %s\r\n"
			     "%s%s%s"
			     "Sec-WebSocket-Location: ws://%s/socket\r\n"
			     "Sec-WebSocket-Protocol: broadway\r\n"
			     "\r\n", accept,
			     origin?"Sec-WebSocket-Origin: ":"", origin?origin:"", origin?"\r\n":"",
			     host);
      g_free (accept);

#ifdef DEBUG_WEBSOCKETS
      g_print ("v7 proto response:\n%s", res);
#endif

      g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
				 res, strlen (res), NULL, NULL, NULL);
      g_free (res);
    }
  else
    {
      g_strfreev (lines);
      send_error (request, 400, "Bad websocket request");
      return;
    }

  socket = g_socket_connection_get_socket (request->connection);
  setsockopt (g_socket_get_fd (socket), IPPROTO_TCP,
	      TCP_NODELAY, (char *) &flag, sizeof(int));

  input = g_new0 (BroadwayInput, 1);
  input->server = request->server;
  input->connection = g_object_ref (request->connection);

  data_buffer = g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (request->data), &data_buffer_size);
  input->buffer = g_byte_array_sized_new (data_buffer_size);
  g_byte_array_append (input->buffer, data_buffer, data_buffer_size);

  input->output =
    broadway_output_new (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)), 0);

  /* This will free and close the data input stream, but we got all the buffered content already */
  http_request_free (request);

  in = g_io_stream_get_input_stream (G_IO_STREAM (input->connection));
  input->source = g_pollable_input_stream_create_source (G_POLLABLE_INPUT_STREAM (in), NULL);
  g_source_set_callback (input->source, (GSourceFunc)input_data_cb, input, NULL);
  g_source_attach (input->source, NULL);

  start (input);

  /* Process any data in the pipe already */
  parse_input (input);

  g_strfreev (lines);
}

static void
start (BroadwayInput *input)
{
  BroadwayServer *server;

  input->active = TRUE;

  server = BROADWAY_SERVER (input->server);

  if (server->output)
    {
      broadway_output_disconnected (server->output);
      broadway_output_flush (server->output);
    }

  if (server->input != NULL)
    {
      broadway_input_free (server->input);
      server->input = NULL;
    }

  server->input = input;

  if (server->output)
    {
      server->saved_serial = broadway_output_get_next_serial (server->output);
      broadway_output_free (server->output);
    }
  server->output = input->output;

  broadway_output_set_next_serial (server->output, server->saved_serial);
  broadway_output_flush (server->output);

  broadway_server_resync_windows (server);

  if (server->pointer_grab_window_id != -1)
    broadway_output_grab_pointer (server->output,
				  server->pointer_grab_window_id,
				  server->pointer_grab_owner_events);

  process_input_messages (server);
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
    start_input (request);
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
  request->server = BROADWAY_SERVER (source_object);
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

BroadwayServer *
broadway_server_new (char *address, int port, GError **error)
{
  BroadwayServer *server;
  GInetAddress *inet_address;
  GSocketAddress *socket_address;

  server = g_object_new (BROADWAY_TYPE_SERVER, NULL);
  server->port = port;
  server->address = g_strdup (address);

  if (address == NULL)
    {
      if (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (server->service),
					    server->port,
					    G_OBJECT (server),
					    error))
	{
	  g_prefix_error (error, "Unable to listen to port %d: ", server->port);
	  return NULL;
	}
    }
  else
    {
      inet_address = g_inet_address_new_from_string (address);
      if (inet_address == NULL)
	{
	  g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid ip address %s: ", address);
	  return NULL;
	}
      socket_address = g_inet_socket_address_new (inet_address, port);
      g_object_unref (inet_address);
      if (!g_socket_listener_add_address (G_SOCKET_LISTENER (server->service),
					  socket_address,
					  G_SOCKET_TYPE_STREAM,
					  G_SOCKET_PROTOCOL_TCP,
					  G_OBJECT (server),
					  NULL,
					  error))
	{
	  g_prefix_error (error, "Unable to listen to %s:%d: ", server->address, server->port);
	  g_object_unref (socket_address);
	  return NULL;
	}
      g_object_unref (socket_address);
    }

  g_signal_connect (server->service, "incoming",
		    G_CALLBACK (handle_incoming_connection), NULL);
  return server;
}

BroadwayServer *
broadway_server_on_unix_socket_new (char *address, GError **error)
{
  BroadwayServer *server;
  GSocketAddress *socket_address = NULL;

  server = g_object_new (BROADWAY_TYPE_SERVER, NULL);
  server->port = -1;
  server->address = g_strdup (address);

  if (address == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Unspecified unix domain socket address");
      g_object_unref (server);
      return NULL;
    }
  else
    {
#ifdef HAVE_GIO_UNIX
      socket_address = g_unix_socket_address_new (address);
#endif
      if (socket_address == NULL)
	{
	  g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid unix domain socket address %s: ", address);
	  g_object_unref (server);
	  return NULL;
	}
      if (!g_socket_listener_add_address (G_SOCKET_LISTENER (server->service),
					  socket_address,
					  G_SOCKET_TYPE_STREAM,
					  G_SOCKET_PROTOCOL_DEFAULT,
					  G_OBJECT (server),
					  NULL,
					  error))
	{
	  g_prefix_error (error, "Unable to listen to %s: ", server->address);
	  g_object_unref (socket_address);
	  g_object_unref (server);
	  return NULL;
	}
      g_object_unref (socket_address);
    }

  g_signal_connect (server->service, "incoming",
		    G_CALLBACK (handle_incoming_connection), NULL);
  return server;
}

guint32
broadway_server_get_last_seen_time (BroadwayServer *server)
{
  broadway_server_consume_all_input (server);
  return (guint32) server->last_seen_time;
}

void
broadway_server_query_mouse (BroadwayServer *server,
			     guint32            *toplevel,
			     gint32             *root_x,
			     gint32             *root_y,
			     guint32            *mask)
{
  if (server->output)
    {
      broadway_server_consume_all_input (server);
      if (root_x)
	*root_x = server->future_root_x;
      if (root_y)
	*root_y = server->future_root_y;
      if (mask)
	*mask = server->future_state;
      if (toplevel)
	*toplevel = server->future_mouse_in_toplevel;
      return;
    }

  /* Fallback when unconnected */
  if (root_x)
    *root_x = server->last_x;
  if (root_y)
    *root_y = server->last_y;
  if (mask)
    *mask = server->last_state;
  if (toplevel)
    *toplevel = server->mouse_in_toplevel_id;
}

void
broadway_server_destroy_window (BroadwayServer *server,
				gint id)
{
  BroadwayWindow *window;

  if (server->mouse_in_toplevel_id == id)
    {
      /* TODO: Send leave + enter event, update cursors, etc */
      server->mouse_in_toplevel_id = 0;
    }

  if (server->pointer_grab_window_id == id)
    server->pointer_grab_window_id = -1;

  if (server->output)
    broadway_output_destroy_surface (server->output,
				     id);

  window = g_hash_table_lookup (server->id_ht,
				GINT_TO_POINTER (id));
  if (window != NULL)
    {
      server->toplevels = g_list_remove (server->toplevels, window);
      g_hash_table_remove (server->id_ht,
			   GINT_TO_POINTER (id));

      if (window->cached_surface_name != NULL)
	g_free (window->cached_surface_name);
      if (window->cached_surface != NULL)
	cairo_surface_destroy (window->cached_surface);

      g_free (window);
    }
}

gboolean
broadway_server_window_show (BroadwayServer *server,
			     gint id)
{
  BroadwayWindow *window;
  gboolean sent = FALSE;

  window = g_hash_table_lookup (server->id_ht,
				GINT_TO_POINTER (id));
  if (window == NULL)
    return FALSE;

  window->visible = TRUE;

  if (server->output)
    {
      broadway_output_show_surface (server->output, window->id);
      sent = TRUE;
    }

  return sent;
}

gboolean
broadway_server_window_hide (BroadwayServer *server,
			     gint id)
{
  BroadwayWindow *window;
  gboolean sent = FALSE;

  window = g_hash_table_lookup (server->id_ht,
				GINT_TO_POINTER (id));
  if (window == NULL)
    return FALSE;

  window->visible = FALSE;

  if (server->mouse_in_toplevel_id == id)
    {
      /* TODO: Send leave + enter event, update cursors, etc */
      server->mouse_in_toplevel_id = 0;
    }

  if (server->pointer_grab_window_id == id)
    server->pointer_grab_window_id = -1;

  if (server->output)
    {
      broadway_output_hide_surface (server->output, window->id);
      sent = TRUE;
    }
  return sent;
}

void
broadway_server_window_raise (BroadwayServer *server,
                              gint id)
{
  BroadwayWindow *window;

  window = g_hash_table_lookup (server->id_ht,
				GINT_TO_POINTER (id));
  if (window == NULL)
    return;

  server->toplevels = g_list_remove (server->toplevels, window);
  server->toplevels = g_list_append (server->toplevels, window);

  if (server->output)
    broadway_output_raise_surface (server->output, window->id);
}

void
broadway_server_set_show_keyboard (BroadwayServer *server,
                                   gboolean show)
{
  server->show_keyboard = show;

  if (server->output)
    {
      broadway_output_set_show_keyboard (server->output, server->show_keyboard);
      broadway_server_flush (server);
   }
}

void
broadway_server_window_lower (BroadwayServer *server,
                              gint id)
{
  BroadwayWindow *window;

  window = g_hash_table_lookup (server->id_ht,
				GINT_TO_POINTER (id));
  if (window == NULL)
    return;

  server->toplevels = g_list_remove (server->toplevels, window);
  server->toplevels = g_list_prepend (server->toplevels, window);

  if (server->output)
    broadway_output_lower_surface (server->output, window->id);
}

void
broadway_server_window_set_transient_for (BroadwayServer *server,
					  gint id, gint parent)
{
  BroadwayWindow *window;

  window = g_hash_table_lookup (server->id_ht,
				GINT_TO_POINTER (id));
  if (window == NULL)
    return;

  window->transient_for = parent;

  if (server->output)
    {
      broadway_output_set_transient_for (server->output, window->id, window->transient_for);
      broadway_server_flush (server);
    }
}

gboolean
broadway_server_has_client (BroadwayServer *server)
{
  return server->output != NULL;
}

void
broadway_server_window_update (BroadwayServer *server,
			       gint id,
			       cairo_surface_t *surface)
{
  BroadwayWindow *window;
  BroadwayBuffer *buffer;

  if (surface == NULL)
    return;

  window = g_hash_table_lookup (server->id_ht,
				GINT_TO_POINTER (id));
  if (window == NULL)
    return;

  g_assert (window->width == cairo_image_surface_get_width (surface));
  g_assert (window->height == cairo_image_surface_get_height (surface));

  buffer = broadway_buffer_create (window->width, window->height,
                                   cairo_image_surface_get_data (surface),
                                   cairo_image_surface_get_stride (surface));

  if (server->output != NULL)
    {
      window->buffer_synced = TRUE;
      broadway_output_put_buffer (server->output, window->id,
                                  window->buffer, buffer);
    }

  if (window->buffer)
    broadway_buffer_destroy (window->buffer);

  window->buffer = buffer;
}

gboolean
broadway_server_window_move_resize (BroadwayServer *server,
				    gint id,
				    gboolean with_move,
				    int x,
				    int y,
				    int width,
				    int height)
{
  BroadwayWindow *window;
  gboolean with_resize;
  gboolean sent = FALSE;

  window = g_hash_table_lookup (server->id_ht,
				GINT_TO_POINTER (id));
  if (window == NULL)
    return FALSE;

  with_resize = width != window->width || height != window->height;
  window->width = width;
  window->height = height;

  if (server->output != NULL)
    {
      broadway_output_move_resize_surface (server->output,
					   window->id,
					   with_move, x, y,
					   with_resize, window->width, window->height);
      sent = TRUE;
    }
  else
    {
      if (with_move)
	{
	  window->x = x;
	  window->y = y;
	}

      fake_configure_notify (server, window);
    }

  return sent;
}

void
broadway_server_focus_window (BroadwayServer *server,
                              gint new_focused_window)
{
  BroadwayInputMsg focus_msg;

  if (server->focused_window_id == new_focused_window)
    return;

  memset (&focus_msg, 0, sizeof (focus_msg));
  focus_msg.base.type = BROADWAY_EVENT_FOCUS;
  focus_msg.base.time = broadway_server_get_last_seen_time (server);
  focus_msg.focus.old_id = server->focused_window_id;
  focus_msg.focus.new_id = new_focused_window;

  broadway_events_got_input (&focus_msg, -1);

  /* Keep track of the new focused window */
  server->focused_window_id = new_focused_window;
}

guint32
broadway_server_grab_pointer (BroadwayServer *server,
			      gint client_id,
			      gint id,
			      gboolean owner_events,
			      guint32 event_mask,
			      guint32 time_)
{
  if (server->pointer_grab_window_id != -1 &&
      time_ != 0 && server->pointer_grab_time > time_)
    return GDK_GRAB_ALREADY_GRABBED;

  if (time_ == 0)
    time_ = server->last_seen_time;

  server->pointer_grab_window_id = id;
  server->pointer_grab_client_id = client_id;
  server->pointer_grab_owner_events = owner_events;
  server->pointer_grab_time = time_;

  if (server->output)
    {
      broadway_output_grab_pointer (server->output,
				    id,
				    owner_events);
      broadway_server_flush (server);
    }

  /* TODO: What about toplevel grab events if we're not connected? */

  return GDK_GRAB_SUCCESS;
}

guint32
broadway_server_ungrab_pointer (BroadwayServer *server,
				guint32    time_)
{
  guint32 serial;

  if (server->pointer_grab_window_id != -1 &&
      time_ != 0 && server->pointer_grab_time > time_)
    return 0;

  /* TODO: What about toplevel grab events if we're not connected? */

  if (server->output)
    {
      serial = broadway_output_ungrab_pointer (server->output);
      broadway_server_flush (server);
    }
  else
    {
      serial = server->saved_serial;
    }

  server->pointer_grab_window_id = -1;

  return serial;
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
#ifdef G_OS_UNIX
  munmap (data->data, data->data_size);
#elif defined(G_OS_WIN32)
  UnmapViewOfFile (data->data);
#endif
  g_free (data);
}

cairo_surface_t *
broadway_server_open_surface (BroadwayServer *server,
			      guint32 id,
			      char *name,
			      int width,
			      int height)
{
  BroadwayWindow *window;
  ShmSurfaceData *data;
  cairo_surface_t *surface;
  gsize size;
  void *ptr;

  window = g_hash_table_lookup (server->id_ht,
				GINT_TO_POINTER (id));
  if (window == NULL)
    return NULL;

  if (window->cached_surface_name != NULL &&
      strcmp (name, window->cached_surface_name) == 0)
    return cairo_surface_reference (window->cached_surface);

  size = width * height * sizeof (guint32);

  ptr = map_named_shm (name, size);

  if (ptr == NULL)
    return NULL;

  data = g_new0 (ShmSurfaceData, 1);

  data->data = ptr;
  data->data_size = size;

  surface = cairo_image_surface_create_for_data ((guchar *)data->data,
						 CAIRO_FORMAT_ARGB32,
						 width, height,
						 width * sizeof (guint32));
  g_assert (surface != NULL);

  cairo_surface_set_user_data (surface, &shm_cairo_key,
			       data, shm_data_unmap);

  if (window->cached_surface_name != NULL)
    g_free (window->cached_surface_name);
  window->cached_surface_name = g_strdup (name);

  if (window->cached_surface != NULL)
    cairo_surface_destroy (window->cached_surface);
  window->cached_surface = cairo_surface_reference (surface);

  return surface;
}

guint32
broadway_server_new_window (BroadwayServer *server,
			    int x,
			    int y,
			    int width,
			    int height,
			    gboolean is_temp)
{
  BroadwayWindow *window;

  window = g_new0 (BroadwayWindow, 1);
  window->id = server->id_counter++;
  window->x = x;
  window->y = y;
  if (x == 0 && y == 0 && !is_temp)
    {
      /* TODO: Better way to know if we should pick default pos */
      window->x = 100;
      window->y = 100;
    }
  window->width = width;
  window->height = height;
  window->is_temp = is_temp;

  g_hash_table_insert (server->id_ht,
		       GINT_TO_POINTER (window->id),
		       window);

  server->toplevels = g_list_append (server->toplevels, window);

  if (server->output)
    broadway_output_new_surface (server->output,
				 window->id,
				 window->x,
				 window->y,
				 window->width,
				 window->height,
				 window->is_temp);
  else
    fake_configure_notify (server, window);

  return window->id;
}

static void
broadway_server_resync_windows (BroadwayServer *server)
{
  GList *l;

  if (server->output == NULL)
    return;

  /* First create all windows */
  for (l = server->toplevels; l != NULL; l = l->next)
    {
      BroadwayWindow *window = l->data;

      if (window->id == 0)
	continue; /* Skip root */

      window->buffer_synced = FALSE;
      broadway_output_new_surface (server->output,
				   window->id,
				   window->x,
				   window->y,
				   window->width,
				   window->height,
				   window->is_temp);
    }

  /* Then do everything that may reference other windows */
  for (l = server->toplevels; l != NULL; l = l->next)
    {
      BroadwayWindow *window = l->data;

      if (window->id == 0)
	continue; /* Skip root */

      if (window->transient_for != -1)
	broadway_output_set_transient_for (server->output, window->id, window->transient_for);
      if (window->visible)
	{
	  broadway_output_show_surface (server->output, window->id);

	  if (window->buffer != NULL)
	    {
	      window->buffer_synced = TRUE;
              broadway_output_put_buffer (server->output, window->id,
                                          NULL, window->buffer);
	    }
	}
    }

  if (server->show_keyboard)
    broadway_output_set_show_keyboard (server->output, TRUE);

  broadway_server_flush (server);
}

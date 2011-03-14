/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkeventsource.h"

#include "gdkinternals.h"

#include <stdlib.h>

static gboolean gdk_event_source_prepare  (GSource     *source,
                                           gint        *timeout);
static gboolean gdk_event_source_check    (GSource     *source);
static gboolean gdk_event_source_dispatch (GSource     *source,
                                           GSourceFunc  callback,
                                           gpointer     user_data);
static void     gdk_event_source_finalize (GSource     *source);

#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

struct _GdkEventSource
{
  GSource source;

  GdkDisplay *display;
  GPollFD event_poll_fd;
};

static GSourceFuncs event_funcs = {
  gdk_event_source_prepare,
  gdk_event_source_check,
  gdk_event_source_dispatch,
  gdk_event_source_finalize
};

static GList *event_sources = NULL;

static gboolean
gdk_event_source_prepare (GSource *source,
                          gint    *timeout)
{
  GdkDisplay *display = ((GdkEventSource*) source)->display;
  gboolean retval;

  GDK_THREADS_ENTER ();

  *timeout = -1;
  retval = (_gdk_event_queue_find_first (display) != NULL);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
gdk_event_source_check (GSource *source)
{
  GdkEventSource *event_source = (GdkEventSource*) source;
  gboolean retval;

  GDK_THREADS_ENTER ();

  if (event_source->event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL);
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

typedef struct {
  int id;
  int root_x;
  int root_y;
  int win_x;
  int win_y;
  guint32 state;
  guint64 time;
} PointerData;

static char *
parse_pointer_data (char *p, PointerData *data)
{
  data->id = strtol (p, &p, 10);
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
  p++; /* Skip , */
  data->time = strtol(p, &p, 10);

  return p;
}


void
_gdk_broadway_events_got_input (GdkDisplay *display,
				const char *message)
{
  GdkBroadwayDisplay *display_broadway = GDK_BROADWAY_DISPLAY (display);
  GdkScreen *screen;
  GdkWindow *root, *window;
  char *p;
  int button, dir, key, mode;
  guint32 serial;
  guint64 time;
  GdkEvent *event = NULL;
  char cmd;
  GList *node;
  PointerData data;

  screen = gdk_display_get_default_screen (display);
  root = gdk_screen_get_root_window (screen);

  p = (char *)message;
  cmd = *p++;
  serial = (guint32)strtol (p, &p, 10);
  p++; /* Skip , */
  switch (cmd) {
  case 'e': /* Enter */
    p = parse_pointer_data (p, &data);
    p++; /* Skip , */
    mode = strtol(p, &p, 10);

    display_broadway->last_x = data.root_x;
    display_broadway->last_y = data.root_y;

    window = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (data.id));

    /* TODO: Unset when it dies */
    display_broadway->mouse_in_toplevel = window;

    if (window)
      {
	event = gdk_event_new (GDK_ENTER_NOTIFY);
	event->crossing.window = g_object_ref (window);
	event->crossing.time = data.time;
	event->crossing.x = data.win_x;
	event->crossing.y = data.win_y;
	event->crossing.x_root = data.root_x;
	event->crossing.y_root = data.root_y;
	event->crossing.state = data.state;
	event->crossing.mode = mode;
	event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	gdk_event_set_device (event, display->core_pointer);

	node = _gdk_event_queue_append (display, event);
	_gdk_windowing_got_event (display, node, event, serial);

	event = gdk_event_new (GDK_FOCUS_CHANGE);
	event->focus_change.window = g_object_ref (window);
	event->focus_change.in = TRUE;
	gdk_event_set_device (event, display->core_pointer);

	node = _gdk_event_queue_append (display, event);
	_gdk_windowing_got_event (display, node, event, serial);
      }
    break;
  case 'l': /* Leave */
    p = parse_pointer_data (p, &data);
    p++; /* Skip , */
    mode = strtol(p, &p, 10);

    display_broadway->last_x = data.root_x;
    display_broadway->last_y = data.root_y;

    window = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (data.id));

    display_broadway->mouse_in_toplevel = NULL;

    if (window)
      {
	event = gdk_event_new (GDK_LEAVE_NOTIFY);
	event->crossing.window = g_object_ref (window);
	event->crossing.time = data.time;
	event->crossing.x = data.win_x;
	event->crossing.y = data.win_y;
	event->crossing.x_root = data.root_x;
	event->crossing.y_root = data.root_y;
	event->crossing.state = data.state;
	event->crossing.mode = mode;
	event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	gdk_event_set_device (event, display->core_pointer);

	node = _gdk_event_queue_append (display, event);
	_gdk_windowing_got_event (display, node, event, serial);

	event = gdk_event_new (GDK_FOCUS_CHANGE);
	event->focus_change.window = g_object_ref (window);
	event->focus_change.in = FALSE;
	gdk_event_set_device (event, display->core_pointer);

	node = _gdk_event_queue_append (display, event);
	_gdk_windowing_got_event (display, node, event, serial);
      }
    break;
  case 'm': /* Mouse move */
    p = parse_pointer_data (p, &data);

    display_broadway->last_x = data.root_x;
    display_broadway->last_y = data.root_y;

    window = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (data.id));

    if (window)
      {
	event = gdk_event_new (GDK_MOTION_NOTIFY);
	event->motion.window = g_object_ref (window);
	event->motion.time = data.time;
	event->motion.x = data.win_x;
	event->motion.y = data.win_y;
	event->motion.x_root = data.root_x;
	event->motion.y_root = data.root_y;
	event->motion.state = data.state;
	gdk_event_set_device (event, display->core_pointer);

	node = _gdk_event_queue_append (display, event);
	_gdk_windowing_got_event (display, node, event, serial);
      }

    break;
  case 'b':
  case 'B':
    p = parse_pointer_data (p, &data);
    p++; /* Skip , */
    button = strtol(p, &p, 10);
    display_broadway->last_x = data.root_x;
    display_broadway->last_y = data.root_y;

    window = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (data.id));

    if (window)
      {
	event = gdk_event_new (cmd == 'b' ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);
	event->button.window = g_object_ref (window);
	event->button.time = data.time;
	event->button.x = data.win_x;
	event->button.y = data.win_y;
	event->button.x_root = data.root_x;
	event->button.y_root = data.root_y;
	event->button.button = button;
	event->button.state = data.state;
	gdk_event_set_device (event, display->core_pointer);

	node = _gdk_event_queue_append (display, event);
	_gdk_windowing_got_event (display, node, event, serial);
      }

    break;
  case 's':
    p = parse_pointer_data (p, &data);
    p++; /* Skip , */
    dir = strtol(p, &p, 10);
    display_broadway->last_x = data.root_x;
    display_broadway->last_y = data.root_y;

    window = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (data.id));

    if (window)
      {
	event = gdk_event_new (GDK_SCROLL);
	event->scroll.window = g_object_ref (window);
	event->scroll.time = data.time;
	event->scroll.x = data.win_x;
	event->scroll.y = data.win_y;
	event->scroll.x_root = data.root_x;
	event->scroll.y_root = data.root_y;
	event->scroll.direction = dir == 0 ? GDK_SCROLL_UP : GDK_SCROLL_DOWN;
	gdk_event_set_device (event, display->core_pointer);

	node = _gdk_event_queue_append (display, event);
	_gdk_windowing_got_event (display, node, event, serial);
      }

    break;
  case 'k':
  case 'K':
    key = strtol(p, &p, 10);
    p++; /* Skip , */
    time = strtol(p, &p, 10);

    window = display_broadway->mouse_in_toplevel;

    if (window)
      {
	event = gdk_event_new (cmd == 'k' ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
	event->key.window = g_object_ref (window);
	event->key.time = time;
	event->key.keyval = key;
	event->key.length = 0;
	gdk_event_set_device (event, display->core_pointer);

	node = _gdk_event_queue_append (display, event);
	_gdk_windowing_got_event (display, node, event, serial);
      }

    break;
  case 'g':
  case 'u':
    _gdk_display_device_grab_update (display, display->core_pointer, NULL, serial);
    break;
  case 'q':
    g_printerr ("Got unexpected query pointer reply w serial %d\n", serial);
    break;
  default:
    g_printerr ("Unknown input command %s\n", message);
    break;
  }
}

void
_gdk_broadway_display_queue_events (GdkDisplay *display)
{
}

static gboolean
gdk_event_source_dispatch (GSource     *source,
                           GSourceFunc  callback,
                           gpointer     user_data)
{
  GdkDisplay *display = ((GdkEventSource*) source)->display;
  GdkEvent *event;

  GDK_THREADS_ENTER ();

  event = gdk_display_get_event (display);

  if (event)
    {
      _gdk_event_emit (event);

      gdk_event_free (event);
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}

static void
gdk_event_source_finalize (GSource *source)
{
  GdkEventSource *event_source = (GdkEventSource *)source;

  event_sources = g_list_remove (event_sources, event_source);
}

GSource *
_gdk_broadway_event_source_new (GdkDisplay *display)
{
  GSource *source;
  GdkEventSource *event_source;
  char *name;

  source = g_source_new (&event_funcs, sizeof (GdkEventSource));
  name = g_strdup_printf ("GDK Broadway Event source (%s)",
			  gdk_display_get_name (display));
  g_source_set_name (source, name);
  g_free (name);
  event_source = (GdkEventSource *) source;
  event_source->display = display;

  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  event_sources = g_list_prepend (event_sources, source);

  return source;
}

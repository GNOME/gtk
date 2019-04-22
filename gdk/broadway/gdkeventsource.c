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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkeventsource.h"
#include "gdkseat.h"

#include "gdksurfaceprivate.h"
#include "gdkframeclockprivate.h"

#include <stdlib.h>

static gboolean gdk_event_source_prepare  (GSource     *source,
                                           gint        *timeout);
static gboolean gdk_event_source_check    (GSource     *source);
static gboolean gdk_event_source_dispatch (GSource     *source,
                                           GSourceFunc  callback,
                                           gpointer     user_data);
static void     gdk_event_source_finalize (GSource     *source);

#define HAS_FOCUS(toplevel)                                     \
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

  *timeout = -1;

  retval = (_gdk_event_queue_find_first (display) != NULL);

  return retval;
}

static gboolean
gdk_event_source_check (GSource *source)
{
  GdkEventSource *event_source = (GdkEventSource*) source;
  gboolean retval;

  if (event_source->display->event_pause_count > 0 ||
      event_source->event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL);
  else
    retval = FALSE;

  return retval;
}

void
_gdk_broadway_events_got_input (GdkDisplay *display,
                                BroadwayInputMsg *message)
{
  GdkBroadwayDisplay *display_broadway;
  GdkSeat *seat;
  GdkSurface *surface;
  GdkEvent *event = NULL;
  GList *node;

  display_broadway = GDK_BROADWAY_DISPLAY (display);
  seat = gdk_display_get_default_seat (display);

  switch (message->base.type) {
  case BROADWAY_EVENT_ENTER:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_event_new (GDK_ENTER_NOTIFY);
        event->any.surface = g_object_ref (surface);
        event->crossing.time = message->base.time;
        event->crossing.x = message->pointer.win_x;
        event->crossing.y = message->pointer.win_y;
        event->crossing.x_root = message->pointer.root_x;
        event->crossing.y_root = message->pointer.root_y;
        event->crossing.state = message->pointer.state;
        event->crossing.mode = message->crossing.mode;
        event->crossing.detail = GDK_NOTIFY_ANCESTOR;
        gdk_event_set_device (event, gdk_seat_get_pointer (seat));

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }
    break;
  case BROADWAY_EVENT_LEAVE:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_event_new (GDK_LEAVE_NOTIFY);
        event->any.surface = g_object_ref (surface);
        event->crossing.time = message->base.time;
        event->crossing.x = message->pointer.win_x;
        event->crossing.y = message->pointer.win_y;
        event->crossing.x_root = message->pointer.root_x;
        event->crossing.y_root = message->pointer.root_y;
        event->crossing.state = message->pointer.state;
        event->crossing.mode = message->crossing.mode;
        event->crossing.detail = GDK_NOTIFY_ANCESTOR;
        gdk_event_set_device (event, gdk_seat_get_pointer (seat));

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }
    break;
  case BROADWAY_EVENT_POINTER_MOVE:
    if (_gdk_broadway_moveresize_handle_event (display, message))
      break;

    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_event_new (GDK_MOTION_NOTIFY);
        event->any.surface = g_object_ref (surface);
        event->motion.time = message->base.time;
        event->motion.x = message->pointer.win_x;
        event->motion.y = message->pointer.win_y;
        event->motion.x_root = message->pointer.root_x;
        event->motion.y_root = message->pointer.root_y;
        event->motion.state = message->pointer.state;
        gdk_event_set_device (event, gdk_seat_get_pointer (seat));

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_BUTTON_PRESS:
  case BROADWAY_EVENT_BUTTON_RELEASE:
    if (message->base.type != BROADWAY_EVENT_BUTTON_PRESS &&
        _gdk_broadway_moveresize_handle_event (display, message))
      break;

    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_event_new (message->base.type == BROADWAY_EVENT_BUTTON_PRESS ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);
        event->any.surface = g_object_ref (surface);
        event->button.time = message->base.time;
        event->button.x = message->pointer.win_x;
        event->button.y = message->pointer.win_y;
        event->button.x_root = message->pointer.root_x;
        event->button.y_root = message->pointer.root_y;
        event->button.button = message->button.button;
        event->button.state = message->pointer.state;
        gdk_event_set_device (event, gdk_seat_get_pointer (seat));

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_SCROLL:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->pointer.event_surface_id));
    if (surface)
      {
        event = gdk_event_new (GDK_SCROLL);
        event->any.surface = g_object_ref (surface);
        event->scroll.time = message->base.time;
        event->scroll.x = message->pointer.win_x;
        event->scroll.y = message->pointer.win_y;
        event->scroll.x_root = message->pointer.root_x;
        event->scroll.y_root = message->pointer.root_y;
        event->scroll.direction = message->scroll.dir == 0 ? GDK_SCROLL_UP : GDK_SCROLL_DOWN;
        gdk_event_set_device (event, gdk_seat_get_pointer (seat));

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_TOUCH:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->touch.event_surface_id));
    if (surface)
      {
        GdkEventType event_type = 0;

        switch (message->touch.touch_type) {
        case 0:
          event_type = GDK_TOUCH_BEGIN;
          break;
        case 1:
          event_type = GDK_TOUCH_UPDATE;
          break;
        case 2:
          event_type = GDK_TOUCH_END;
          break;
        default:
          g_printerr ("_gdk_broadway_events_got_input - Unknown touch type %d\n", message->touch.touch_type);
        }

        if (event_type != GDK_TOUCH_BEGIN &&
            message->touch.is_emulated && _gdk_broadway_moveresize_handle_event (display, message))
          break;

        event = gdk_event_new (event_type);
        event->any.surface = g_object_ref (surface);
        event->touch.sequence = GUINT_TO_POINTER(message->touch.sequence_id);
        event->touch.emulating_pointer = message->touch.is_emulated;
        event->touch.time = message->base.time;
        event->touch.x = message->touch.win_x;
        event->touch.y = message->touch.win_y;
        event->touch.x_root = message->touch.root_x;
        event->touch.y_root = message->touch.root_y;
        event->touch.state = message->touch.state;

        gdk_event_set_device (event, gdk_seat_get_pointer (seat));
        {
          GList *devices;
          devices = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_TOUCH);
          if (devices)
            gdk_event_set_source_device (event, GDK_DEVICE (devices->data));
          g_list_free (devices);
        }
        if (message->touch.is_emulated)
          gdk_event_set_pointer_emulated (event, TRUE);

        if (event_type == GDK_TOUCH_BEGIN || event_type == GDK_TOUCH_UPDATE)
          event->touch.state |= GDK_BUTTON1_MASK;

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_KEY_PRESS:
  case BROADWAY_EVENT_KEY_RELEASE:
    surface = g_hash_table_lookup (display_broadway->id_ht,
                                  GINT_TO_POINTER (message->key.surface_id));
    if (surface)
      {
        event = gdk_event_new (message->base.type == BROADWAY_EVENT_KEY_PRESS ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
        event->any.surface = g_object_ref (surface);
        event->key.time = message->base.time;
        event->key.keyval = message->key.key;
        event->key.state = message->key.state;
        event->key.hardware_keycode = message->key.key;
        gdk_event_set_scancode (event, message->key.key);
        gdk_event_set_device (event, gdk_seat_get_keyboard (seat));

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }

    break;
  case BROADWAY_EVENT_GRAB_NOTIFY:
  case BROADWAY_EVENT_UNGRAB_NOTIFY:
    _gdk_display_device_grab_update (display, gdk_seat_get_pointer (seat), gdk_seat_get_pointer (seat), message->base.serial);
    break;

  case BROADWAY_EVENT_CONFIGURE_NOTIFY:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->configure_notify.id));
    if (surface)
      {
        surface->x = message->configure_notify.x;
        surface->y = message->configure_notify.y;

        event = gdk_event_new (GDK_CONFIGURE);
        event->any.surface = g_object_ref (surface);
        event->configure.x = message->configure_notify.x;
        event->configure.y = message->configure_notify.y;
        event->configure.width = message->configure_notify.width;
        event->configure.height = message->configure_notify.height;

        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);

        if (surface->resize_count >= 1)
          {
            surface->resize_count -= 1;

            if (surface->resize_count == 0)
              _gdk_broadway_moveresize_configure_done (display, surface);
          }
      }
    break;

  case BROADWAY_EVENT_ROUNDTRIP_NOTIFY:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->roundtrip_notify.id));
    if (surface)
      _gdk_broadway_roundtrip_notify (surface, message->roundtrip_notify.tag, message->roundtrip_notify.local);
    break;

  case BROADWAY_EVENT_SCREEN_SIZE_CHANGED:
    _gdk_broadway_display_size_changed (display, &message->screen_resize_notify);
    break;

  case BROADWAY_EVENT_FOCUS:
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->focus.old_id));
    if (surface)
      {
        event = gdk_event_new (GDK_FOCUS_CHANGE);
        event->any.surface = g_object_ref (surface);
        event->focus_change.in = FALSE;
        gdk_event_set_device (event, gdk_seat_get_pointer (seat));
        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }
    surface = g_hash_table_lookup (display_broadway->id_ht, GINT_TO_POINTER (message->focus.new_id));
    if (surface)
      {
        event = gdk_event_new (GDK_FOCUS_CHANGE);
        event->any.surface = g_object_ref (surface);
        event->focus_change.in = TRUE;
        gdk_event_set_device (event, gdk_seat_get_pointer (seat));
        node = _gdk_event_queue_append (display, event);
        _gdk_windowing_got_event (display, node, event, message->base.serial);
      }
    break;

  default:
    g_printerr ("_gdk_broadway_events_got_input - Unknown input command %c\n", message->base.type);
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

  event = gdk_display_get_event (display);

  if (event)
    {
      _gdk_event_emit (event);

      g_object_unref (event);
    }

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

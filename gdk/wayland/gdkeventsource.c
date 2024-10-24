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

#include "gdkprivate-wayland.h"

#include "gdkeventsprivate.h"

#include <unistd.h>
#include <errno.h>

typedef struct _GdkWaylandEventSource GdkWaylandEventSource;
typedef struct _GdkWaylandPollSource GdkWaylandPollSource;

struct _GdkWaylandEventSource
{
  GSource source;
  GdkWaylandDisplay *display;
};

struct _GdkWaylandPollSource
{
  GSource source;
  GPollFD pfd;
  GdkWaylandDisplay *display;
  guint reading : 1;
  guint can_dispatch : 1;
};

/* If we should try wl_display_dispatch_pending() before
 * polling the Wayland fd
 */
static gboolean
gdk_wayland_display_can_dispatch (GdkWaylandDisplay *display_wayland)
{
  GdkWaylandPollSource *poll_source = (GdkWaylandPollSource *) display_wayland->poll_source;

  return poll_source->can_dispatch;
}

/* If we still have events to process and don't need to poll */
static gboolean
gdk_wayland_display_has_events_pending (GdkWaylandDisplay *display_wayland)
{
  return gdk_wayland_display_can_dispatch (display_wayland) ||
         _gdk_event_queue_find_first (GDK_DISPLAY (display_wayland)) != NULL;
}

static gboolean
gdk_wayland_event_source_prepare (GSource *base,
                                  int     *timeout)
{
  GdkWaylandEventSource *source = (GdkWaylandEventSource *) base;

  *timeout = -1;

  return gdk_wayland_display_has_events_pending (source->display);
}

static gboolean
gdk_wayland_event_source_check (GSource *base)
{
  GdkWaylandEventSource *source = (GdkWaylandEventSource *) base;

  return gdk_wayland_display_has_events_pending (source->display);
}

static gboolean
gdk_wayland_event_source_dispatch (GSource     *base,
                                   GSourceFunc  callback,
                                   gpointer     data)
{
  GdkWaylandEventSource *source = (GdkWaylandEventSource *) base;
  GdkEvent *event;

  event = gdk_display_get_event (GDK_DISPLAY (source->display));

  if (event)
    {
      _gdk_event_emit (event);

      gdk_event_unref (event);
    }

  return G_SOURCE_CONTINUE;
}

static void
gdk_wayland_event_source_finalize (GSource *base)
{
}

static GSourceFuncs gdk_wayland_event_source_funcs = {
  gdk_wayland_event_source_prepare,
  gdk_wayland_event_source_check,
  gdk_wayland_event_source_dispatch,
  gdk_wayland_event_source_finalize
};

static gboolean
gdk_wayland_poll_source_prepare (GSource *base,
                                 int     *timeout)
{
  GdkWaylandPollSource *source = (GdkWaylandPollSource *) base;
  GdkWaylandDisplay *display = (GdkWaylandDisplay *) source->display;
  GList *l;

  *timeout = -1;

  if (gdk_wayland_display_has_events_pending (source->display))
    return FALSE;

  /* wl_display_prepare_read() needs to be balanced with either
   * wl_display_read_events() or wl_display_cancel_read()
   * (in gdk_wayland_event_source_check() */
  if (source->reading)
    return FALSE;

  /* if prepare_read() returns non-zero, there are events to be dispatched */
  if (wl_display_prepare_read (display->wl_display) != 0)
    {
      source->can_dispatch = TRUE;
      return TRUE;
    }

  /* We need to check whether there are pending events on the surface queues as well,
   * but we also need to make sure to only have one active "read" in the end,
   * or none if we immediately return TRUE, as multiple reads expect reads from
   * as many threads.
   */
  for (l = display->event_queues; l; l = l->next)
    {
      struct wl_event_queue *queue = l->data;

      if (wl_display_prepare_read_queue (display->wl_display, queue) != 0)
        {
          source->can_dispatch = TRUE;
          /* cancel the read from before the for loop */
          wl_display_cancel_read (display->wl_display);
          return TRUE;
        }
      wl_display_cancel_read (display->wl_display);
    }

  source->reading = TRUE;

  if (wl_display_flush (display->wl_display) < 0)
    {
      g_message ("Error flushing display: %s", g_strerror (errno));
      _exit (1);
    }

  return FALSE;
}

static gboolean
gdk_wayland_poll_source_check (GSource *base)
{
  GdkWaylandPollSource *source = (GdkWaylandPollSource *) base;
  GdkWaylandDisplay *display_wayland = (GdkWaylandDisplay *) source->display;

  /* read the events from the wayland fd into their respective queues if we have data */
  if (source->reading)
    {
      if (source->pfd.revents & (G_IO_ERR | G_IO_HUP))
        {
          g_message ("Lost connection to Wayland compositor.");
          _exit (1);
        }
      if (source->pfd.revents & G_IO_IN)
        {
          if (wl_display_read_events (display_wayland->wl_display) < 0)
            {
              g_message ("Error reading events from display: %s", g_strerror (errno));
              _exit (1);
            }
          source->pfd.revents = 0;
          source->can_dispatch = TRUE;
        }
      else
        wl_display_cancel_read (display_wayland->wl_display);
      source->reading = FALSE;
    }

  return FALSE;
}

static gboolean
gdk_wayland_poll_source_dispatch (GSource     *base,
                                  GSourceFunc  callback,
                                  gpointer     data)
{
  return G_SOURCE_CONTINUE;
}

static void
gdk_wayland_poll_source_finalize (GSource *base)
{
  GdkWaylandPollSource *source = (GdkWaylandPollSource *) base;

  if (source->reading)
    wl_display_cancel_read (source->display->wl_display);
  source->reading = FALSE;
}

static GSourceFuncs gdk_wayland_poll_source_funcs = {
  gdk_wayland_poll_source_prepare,
  gdk_wayland_poll_source_check,
  gdk_wayland_poll_source_dispatch,
  gdk_wayland_poll_source_finalize
};

void
_gdk_wayland_display_deliver_event (GdkDisplay *display,
                                    GdkEvent   *event)
{
  GList *node;

  node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event,
                            _gdk_display_get_next_serial (display));
}

void
gdk_wayland_display_install_gsources (GdkWaylandDisplay *display_wayland)
{
  GdkDisplay *display = GDK_DISPLAY (display_wayland);
  GSource *source;
  GdkWaylandEventSource *event_source;
  GdkWaylandPollSource *poll_source;
  char *name;

  /* SOURCE 1 */
  source = g_source_new (&gdk_wayland_event_source_funcs,
			 sizeof (GdkWaylandEventSource));
  display_wayland->event_source = source;
  event_source = (GdkWaylandEventSource *) source;
  name = g_strdup_printf ("GDK Wayland Event source (%s)",
                          gdk_display_get_name (display));
  g_source_set_name (source, name);
  g_free (name);

  event_source->display = display_wayland;

  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);
  
  /* SOURCE 2 */
  source = g_source_new (&gdk_wayland_poll_source_funcs,
			 sizeof (GdkWaylandPollSource));
  display_wayland->poll_source = source;
  poll_source = (GdkWaylandPollSource *) source;
  name = g_strdup_printf ("GDK Wayland Poll source (%s)",
                          gdk_display_get_name (display));
  g_source_set_name (source, name);
  g_free (name);

  poll_source->display = display_wayland;
  poll_source->pfd.fd = wl_display_get_fd (display_wayland->wl_display);
  poll_source->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
  g_source_add_poll (source, &poll_source->pfd);

  /* We must guarantee to ALWAYS be called and called FIRST after
   * every poll - or rather: after every prepare().
   * Any other source might call Wayland functions and in turn
   * block while waiting for us.
   * And GSource has no after_pool() vfunc, check() is not guaranteed
   * to be called.
   */
  g_source_set_priority (source, G_MININT);
  g_source_attach (source, NULL);
}

void
gdk_wayland_display_uninstall_gsources (GdkWaylandDisplay *display_wayland)
{
  if (display_wayland->event_source)
    {
      g_source_destroy (display_wayland->event_source);
      g_source_unref (display_wayland->event_source);
      display_wayland->event_source = NULL;
    }
  if (display_wayland->poll_source)
    {
      g_source_destroy (display_wayland->poll_source);
      g_source_unref (display_wayland->poll_source);
      display_wayland->poll_source = NULL;
    }
}

void
_gdk_wayland_display_queue_events (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandPollSource *poll_source;
  GList *l;

  if (wl_display_dispatch_pending (display_wayland->wl_display) < 0)
    {
      g_message ("Error %d (%s) dispatching to Wayland display.",
                 errno, g_strerror (errno));
      _exit (1);
    }

  for (l = display_wayland->event_queues; l; l = l->next)
    {
      struct wl_event_queue *queue = l->data;

      if (wl_display_dispatch_queue_pending (display_wayland->wl_display, queue) < 0)
        {
          g_message ("Error %d (%s) dispatching to Wayland display.",
                     errno, g_strerror (errno));
          _exit (1);
        }
    }

  poll_source = (GdkWaylandPollSource *) display_wayland->poll_source;
  poll_source->can_dispatch = FALSE;
}

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

#include "gdkinternals.h"
#include "gdkprivate-wayland.h"

typedef struct _GdkWaylandEventSource {
  GSource source;
  GPollFD pfd;
  uint32_t mask;
  GdkDisplay *display;
} GdkWaylandEventSource;

static GList *event_sources = NULL;

static gboolean
gdk_event_source_prepare(GSource *base, gint *timeout)
{
  GdkWaylandEventSource *source = (GdkWaylandEventSource *) base;
  GdkWaylandDisplay *display = (GdkWaylandDisplay *) source->display;

  *timeout = -1;

  if (source->display->event_pause_count > 0)
    return FALSE;

  /* We have to add/remove the GPollFD if we want to update our
   * poll event mask dynamically.  Instead, let's just flush all
   * write on idle instead, which is what this amounts to. */

  if (_gdk_event_queue_find_first (source->display) != NULL)
    return TRUE;

  wl_display_flush(display->wl_display);

  return FALSE;
}

static gboolean
gdk_event_source_check(GSource *base)
{
  GdkWaylandEventSource *source = (GdkWaylandEventSource *) base;

  if (source->pfd.revents & (G_IO_ERR | G_IO_HUP))
    g_error ("Lost connection to wayland compositor");

  if (source->display->event_pause_count > 0)
    return FALSE;

  return _gdk_event_queue_find_first (source->display) != NULL ||
    source->pfd.revents;
}

static gboolean
gdk_event_source_dispatch(GSource *base,
			  GSourceFunc callback,
			  gpointer data)
{
  GdkWaylandEventSource *source = (GdkWaylandEventSource *) base;
  GdkDisplay *display = source->display;
  GdkEvent *event;

  gdk_threads_enter ();

  event = gdk_display_get_event (display);

  if (event)
    {
      _gdk_event_emit (event);

      gdk_event_free (event);
    }

  gdk_threads_leave ();

  return TRUE;
}

static void
gdk_event_source_finalize (GSource *source)
{
  event_sources = g_list_remove (event_sources, source);
}

static GSourceFuncs wl_glib_source_funcs = {
  gdk_event_source_prepare,
  gdk_event_source_check,
  gdk_event_source_dispatch,
  gdk_event_source_finalize
};

void
_gdk_wayland_display_deliver_event (GdkDisplay *display, GdkEvent *event)
{
  GList *node;

  node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event,
                            _gdk_display_get_next_serial (display));
}

GSource *
_gdk_wayland_display_event_source_new (GdkDisplay *display)
{
  GSource *source;
  GdkWaylandEventSource *wl_source;
  GdkWaylandDisplay *display_wayland;
  char *name;

  source = g_source_new (&wl_glib_source_funcs,
			 sizeof (GdkWaylandEventSource));
  name = g_strdup_printf ("GDK Wayland Event source (%s)", "display name");
  g_source_set_name (source, name);
  g_free (name);
  wl_source = (GdkWaylandEventSource *) source;

  display_wayland = GDK_WAYLAND_DISPLAY (display);
  wl_source->display = display;
  wl_source->pfd.fd = wl_display_get_fd(display_wayland->wl_display);
  wl_source->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
  g_source_add_poll(source, &wl_source->pfd);

  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  event_sources = g_list_prepend (event_sources, source);

  return source;
}

void
_gdk_wayland_display_queue_events (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland;
  GdkWaylandEventSource *source;

  display_wayland = GDK_WAYLAND_DISPLAY (display);
  source = (GdkWaylandEventSource *) display_wayland->event_source;
  if (source->pfd.revents)
    {
	wl_display_dispatch(display_wayland->wl_display);
	source->pfd.revents = 0;
    }
}

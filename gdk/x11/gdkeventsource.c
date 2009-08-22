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

#include "gdkeventsource.h"
#include "gdkinternals.h"
#include "gdkx.h"

static gboolean gdk_event_source_prepare  (GSource     *source,
                                           gint        *timeout);
static gboolean gdk_event_source_check    (GSource     *source);
static gboolean gdk_event_source_dispatch (GSource     *source,
                                           GSourceFunc  callback,
                                           gpointer     user_data);
static void     gdk_event_source_finalize (GSource     *source);

struct _GdkEventSource
{
  GSource source;

  GdkDisplay *display;
  GPollFD event_poll_fd;
  GList *translators;
};

static GSourceFuncs event_funcs = {
  gdk_event_source_prepare,
  gdk_event_source_check,
  gdk_event_source_dispatch,
  gdk_event_source_finalize
};

static GList *event_sources = NULL;

static gint
gdk_event_apply_filters (XEvent *xevent,
			 GdkEvent *event,
			 GList *filters)
{
  GList *tmp_list;
  GdkFilterReturn result;

  tmp_list = filters;

  while (tmp_list)
    {
      GdkEventFilter *filter = (GdkEventFilter*) tmp_list->data;

      tmp_list = tmp_list->next;
      result = filter->function (xevent, event, filter->data);

      if (result !=  GDK_FILTER_CONTINUE)
	return result;
    }

  return GDK_FILTER_CONTINUE;
}

static GdkWindow *
gdk_event_source_get_filter_window (GdkEventSource *event_source,
                                    XEvent         *xevent)
{
  GdkWindow *window;

  window = gdk_window_lookup_for_display (event_source->display,
                                          xevent->xany.window);

  if (window && !GDK_IS_WINDOW (window))
    window = NULL;

  return window;
}

static GdkEvent *
gdk_event_source_translate_event (GdkEventSource *event_source,
                                  XEvent         *xevent)
{
  GdkEvent *event = NULL;
  GList *list = event_source->translators;
  GdkFilterReturn result;
  GdkWindow *filter_window;

  /* Run default filters */
  if (_gdk_default_filters)
    {
      /* Apply global filters */

      result = gdk_event_apply_filters (xevent, event,
                                        _gdk_default_filters);

      if (result == GDK_FILTER_REMOVE)
        return NULL;
      else if (result == GDK_FILTER_TRANSLATE)
        return event;
    }

  filter_window = gdk_event_source_get_filter_window (event_source, xevent);

  if (filter_window)
    {
      /* Apply per-window filters */
      GdkWindowObject *filter_private = (GdkWindowObject *) filter_window;
      GdkFilterReturn result;

      if (filter_private->filters)
	{
	  g_object_ref (filter_window);

	  result = gdk_event_apply_filters (xevent, event,
					    filter_private->filters);

	  g_object_unref (filter_window);

          if (result == GDK_FILTER_REMOVE)
            return NULL;
          else if (result == GDK_FILTER_TRANSLATE)
            return event;
	}
    }

  while (list && !event)
    {
      GdkEventTranslator *translator = list->data;

      list = list->next;
      event = gdk_event_translator_translate (translator,
                                              event_source->display,
                                              xevent);
    }

  return event;
}

static gboolean
gdk_check_xpending (GdkDisplay *display)
{
  return XPending (GDK_DISPLAY_XDISPLAY (display));
}

static gboolean
gdk_event_source_prepare (GSource *source,
                          gint    *timeout)
{
  GdkDisplay *display = ((GdkEventSource*) source)->display;
  gboolean retval;

  GDK_THREADS_ENTER ();

  *timeout = -1;
  retval = (_gdk_event_queue_find_first (display) != NULL ||
	    gdk_check_xpending (display));

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
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL ||
	      gdk_check_xpending (event_source->display));
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

void
_gdk_events_queue (GdkDisplay *display)
{
  GdkEvent *event;
  XEvent xevent;
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  GdkEventSource *event_source;
  GdkDisplayX11 *display_x11;

  display_x11 = GDK_DISPLAY_X11 (display);
  event_source = (GdkEventSource *) display_x11->event_source;

  while (!_gdk_event_queue_find_first(display) && XPending (xdisplay))
    {
      XNextEvent (xdisplay, &xevent);

      switch (xevent.type)
	{
	case KeyPress:
	case KeyRelease:
	  break;
	default:
	  if (XFilterEvent (&xevent, None))
	    continue;
	}

      event = gdk_event_source_translate_event (event_source, &xevent);

      if (event)
        {
          GList *node;

          node = _gdk_event_queue_append (display, event);
          _gdk_windowing_got_event (display, node, event, xevent.xany.serial);
        }
    }
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
      if (_gdk_event_func)
	(*_gdk_event_func) (event, _gdk_event_data);

      gdk_event_free (event);
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}

static void
gdk_event_source_finalize (GSource *source)
{
  event_sources = g_list_remove (event_sources, source);
}

GSource *
gdk_event_source_new (GdkDisplay *display)
{
  GSource *source;
  GdkEventSource *event_source;
  GdkDisplayX11 *display_x11;
  int connection_number;

  source = g_source_new (&event_funcs, sizeof (GdkEventSource));
  event_source = (GdkEventSource *) source;
  event_source->display = display;

  display_x11 = GDK_DISPLAY_X11 (display);
  connection_number = ConnectionNumber (display_x11->xdisplay);

  event_source->event_poll_fd.fd = connection_number;
  event_source->event_poll_fd.events = G_IO_IN;
  g_source_add_poll (source, &event_source->event_poll_fd);

  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  event_sources = g_list_prepend (event_sources, source);

  return source;
}

void
gdk_event_source_add_translator (GdkEventSource     *source,
                                 GdkEventTranslator *translator)
{
  g_return_if_fail (GDK_IS_EVENT_TRANSLATOR (translator));

  source->translators = g_list_prepend (source->translators, translator);
}

void
gdk_event_source_select_events (GdkEventSource *source,
                                Window          window,
                                GdkEventMask    event_mask,
                                unsigned int    extra_x_mask)
{
  unsigned int xmask = extra_x_mask;
  GList *list;
  gint i;

  list = source->translators;

  while (list)
    {
      GdkEventTranslator *translator = list->data;
      GdkEventMask translator_mask, mask;

      translator_mask = gdk_event_translator_get_handled_events (translator);
      mask = event_mask & translator_mask;

      if (mask != 0)
        {
          gdk_event_translator_select_window_events (translator, window, mask);
          event_mask &= ~(mask);
        }

      list = list->next;
    }

  for (i = 0; i < _gdk_nenvent_masks; i++)
    {
      if (event_mask & (1 << (i + 1)))
        xmask |= _gdk_event_mask_table[i];
    }

  XSelectInput (GDK_DISPLAY_XDISPLAY (source->display), window, xmask);
}

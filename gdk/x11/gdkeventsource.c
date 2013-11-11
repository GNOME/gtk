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

#include "gdkinternals.h"
#include "gdkwindow-x11.h"
#include "gdkprivate-x11.h"


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
gdk_event_apply_filters (XEvent    *xevent,
			 GdkEvent  *event,
			 GdkWindow *window)
{
  GList *tmp_list;
  GdkFilterReturn result;

  if (window == NULL)
    tmp_list = _gdk_default_filters;
  else
    tmp_list = window->filters;

  while (tmp_list)
    {
      GdkEventFilter *filter = (GdkEventFilter*) tmp_list->data;
      GList *node;

      if ((filter->flags & GDK_EVENT_FILTER_REMOVED) != 0)
        {
          tmp_list = tmp_list->next;
          continue;
        }

      filter->ref_count++;
      result = filter->function (xevent, event, filter->data);

      /* Protect against unreffing the filter mutating the list */
      node = tmp_list->next;

      _gdk_event_filter_unref (window, filter);

      tmp_list = node;

      if (result != GDK_FILTER_CONTINUE)
        return result;
    }

  return GDK_FILTER_CONTINUE;
}

static GdkWindow *
gdk_event_source_get_filter_window (GdkEventSource      *event_source,
                                    XEvent              *xevent,
                                    GdkEventTranslator **event_translator)
{
  GList *list = event_source->translators;
  GdkWindow *window;

  *event_translator = NULL;

  while (list)
    {
      GdkEventTranslator *translator = list->data;

      list = list->next;
      window = _gdk_x11_event_translator_get_window (translator,
                                                     event_source->display,
                                                     xevent);
      if (window)
        {
          *event_translator = translator;
          return window;
        }
    }

  window = gdk_x11_window_lookup_for_display (event_source->display,
                                              xevent->xany.window);

  if (window && !GDK_IS_WINDOW (window))
    window = NULL;

  return window;
}

static void
handle_focus_change (GdkEventCrossing *event)
{
  GdkToplevelX11 *toplevel;
  gboolean focus_in, had_focus;

  toplevel = _gdk_x11_window_get_toplevel (event->window);
  focus_in = (event->type == GDK_ENTER_NOTIFY);

  if (!toplevel || event->detail == GDK_NOTIFY_INFERIOR)
    return;

  toplevel->has_pointer = focus_in;

  if (!event->focus || toplevel->has_focus_window)
    return;

  had_focus = HAS_FOCUS (toplevel);
  toplevel->has_pointer_focus = focus_in;

  if (HAS_FOCUS (toplevel) != had_focus)
    {
      GdkEvent *focus_event;

      focus_event = gdk_event_new (GDK_FOCUS_CHANGE);
      focus_event->focus_change.window = g_object_ref (event->window);
      focus_event->focus_change.send_event = FALSE;
      focus_event->focus_change.in = focus_in;
      gdk_event_set_device (focus_event, gdk_event_get_device ((GdkEvent *) event));

      gdk_event_put (focus_event);
      gdk_event_free (focus_event);
    }
}

static GdkEvent *
gdk_event_source_translate_event (GdkEventSource *event_source,
                                  XEvent         *xevent)
{
  GdkEvent *event = gdk_event_new (GDK_NOTHING);
  GdkFilterReturn result = GDK_FILTER_CONTINUE;
  GdkEventTranslator *event_translator;
  GdkWindow *filter_window;
  Display *dpy;

  dpy = GDK_DISPLAY_XDISPLAY (event_source->display);

#ifdef HAVE_XGENERICEVENTS
  /* Get cookie data here so it's available
   * to every event translator and event filter.
   */
  if (xevent->type == GenericEvent)
    XGetEventData (dpy, &xevent->xcookie);
#endif

  filter_window = gdk_event_source_get_filter_window (event_source, xevent,
                                                      &event_translator);
  if (filter_window)
    event->any.window = g_object_ref (filter_window);

  /* Run default filters */
  if (_gdk_default_filters)
    {
      /* Apply global filters */
      result = gdk_event_apply_filters (xevent, event, NULL);
    }

  if (result == GDK_FILTER_CONTINUE &&
      filter_window && filter_window->filters)
    {
      /* Apply per-window filters */
      result = gdk_event_apply_filters (xevent, event, filter_window);
    }

  if (result != GDK_FILTER_CONTINUE)
    {
#ifdef HAVE_XGENERICEVENTS
      if (xevent->type == GenericEvent)
        XFreeEventData (dpy, &xevent->xcookie);
#endif

      if (result == GDK_FILTER_REMOVE)
        {
          gdk_event_free (event);
          return NULL;
        }
      else /* GDK_FILTER_TRANSLATE */
        return event;
    }

  gdk_event_free (event);
  event = NULL;

  if (event_translator)
    {
      /* Event translator was gotten before in get_filter_window() */
      event = _gdk_x11_event_translator_translate (event_translator,
                                                   event_source->display,
                                                   xevent);
    }
  else
    {
      GList *list = event_source->translators;

      while (list && !event)
        {
          GdkEventTranslator *translator = list->data;

          list = list->next;
          event = _gdk_x11_event_translator_translate (translator,
                                                       event_source->display,
                                                       xevent);
        }
    }

  if (event &&
      (event->type == GDK_ENTER_NOTIFY ||
       event->type == GDK_LEAVE_NOTIFY) &&
      event->crossing.window != NULL)
    {
      /* Handle focusing (in the case where no window manager is running */
      handle_focus_change (&event->crossing);
    }

#ifdef HAVE_XGENERICEVENTS
  if (xevent->type == GenericEvent)
    XFreeEventData (dpy, &xevent->xcookie);
#endif

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

  gdk_threads_enter ();

  *timeout = -1;

  if (display->event_pause_count > 0)
    retval = _gdk_event_queue_find_first (display) != NULL;
  else
    retval = (_gdk_event_queue_find_first (display) != NULL ||
              gdk_check_xpending (display));

  gdk_threads_leave ();

  return retval;
}

static gboolean
gdk_event_source_check (GSource *source)
{
  GdkEventSource *event_source = (GdkEventSource*) source;
  gboolean retval;

  gdk_threads_enter ();

  if (event_source->display->event_pause_count > 0)
    retval = _gdk_event_queue_find_first (event_source->display) != NULL;
  else if (event_source->event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (event_source->display) != NULL ||
              gdk_check_xpending (event_source->display));
  else
    retval = FALSE;

  gdk_threads_leave ();

  return retval;
}

void
_gdk_x11_display_queue_events (GdkDisplay *display)
{
  GdkEvent *event;
  XEvent xevent;
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  GdkEventSource *event_source;
  GdkX11Display *display_x11;

  display_x11 = GDK_X11_DISPLAY (display);
  event_source = (GdkEventSource *) display_x11->event_source;

  while (!_gdk_event_queue_find_first (display) && XPending (xdisplay))
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
  GdkEventSource *event_source = (GdkEventSource *)source;

  g_list_free (event_source->translators);
  event_source->translators = NULL;

  event_sources = g_list_remove (event_sources, source);
}

GSource *
gdk_x11_event_source_new (GdkDisplay *display)
{
  GSource *source;
  GdkEventSource *event_source;
  GdkX11Display *display_x11;
  int connection_number;
  char *name;

  source = g_source_new (&event_funcs, sizeof (GdkEventSource));
  name = g_strdup_printf ("GDK X11 Event source (%s)",
                          gdk_display_get_name (display));
  g_source_set_name (source, name);
  g_free (name);
  event_source = (GdkEventSource *) source;
  event_source->display = display;

  display_x11 = GDK_X11_DISPLAY (display);
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
gdk_x11_event_source_add_translator (GdkEventSource     *source,
                                     GdkEventTranslator *translator)
{
  g_return_if_fail (GDK_IS_EVENT_TRANSLATOR (translator));

  source->translators = g_list_append (source->translators, translator);
}

void
gdk_x11_event_source_select_events (GdkEventSource *source,
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

      translator_mask = _gdk_x11_event_translator_get_handled_events (translator);
      mask = event_mask & translator_mask;

      if (mask != 0)
        {
          _gdk_x11_event_translator_select_window_events (translator, window, mask);
          event_mask &= ~mask;
        }

      list = list->next;
    }

  for (i = 0; i < _gdk_x11_event_mask_table_size; i++)
    {
      if (event_mask & (1 << (i + 1)))
        xmask |= _gdk_x11_event_mask_table[i];
    }

  XSelectInput (GDK_DISPLAY_XDISPLAY (source->display), window, xmask);
}
